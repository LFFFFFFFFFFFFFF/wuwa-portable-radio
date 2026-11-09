#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfplay.h>
#include <mfapi.h>
#include <vector>
#include <string>
#include <algorithm>
#include <stdio.h>
#include "controls.h"
#include "cloud.h"
#include "status.h"
#include "track_info.h"
#include "native_info.h"
#include "native_ui.h"
#include "native_bgm.h"
#include "keys.h"
#include <shellapi.h>

static RadioLog logger;
static HANDLE worker;
static volatile LONG stop_worker, game_thread, session_active, session_end, ack;
static volatile LONG info_inflight, info_force;
static volatile LONG bgm_inflight;
static HHOOK message_hook;
static UINT control_message;
static wchar_t config_path[32768];
static wchar_t import_path[32768], cloud_list_path[32768];
static std::vector<CloudTrack> cloud_tracks;
static size_t cloud_index;
static std::wstring notice;
static ULONGLONG notice_until,requested_at;
static CloudTrack requested_track;
static bool cloud_request_pending;
static void inform(const wchar_t *text) {notice=text;notice_until=GetTickCount64()+6000;}
static void trace(const char *s) { if(logger)logger(s); }
static int read_key(const wchar_t *name,const wchar_t *fallback,int default_key) {
    if(GetPrivateProfileIntW(L"Keys",L"LayoutVersion",0,config_path)!=2)return default_key;
    wchar_t value[32];GetPrivateProfileStringW(L"Keys",name,fallback,value,32,config_path);
    return parse_radio_key(value,default_key);
}
static bool load_cloud_list() {
    cloud_tracks.clear();
    UINT count=GetPrivateProfileIntW(L"NetEase",L"Count",0,cloud_list_path);
    for(UINT i=0;i<count&&i<2000;++i){
        wchar_t section[40],id[40],title[512],artist[512];swprintf_s(section,L"Track%u",i);
        GetPrivateProfileStringW(section,L"Id",L"0",id,40,cloud_list_path);
        GetPrivateProfileStringW(section,L"Name",L"",title,512,cloud_list_path);
        GetPrivateProfileStringW(section,L"Artist",L"",artist,512,cloud_list_path);
        uint64_t value=_wcstoui64(id,NULL,10);if(value)cloud_tracks.push_back({value,title,artist});
    }
    cloud_index=0;
    char line[96];sprintf_s(line,"NetEase imported tracks=%zu",cloud_tracks.size());trace(line);return !cloud_tracks.empty();
}
static bool request_track(size_t index) {
    if(index>=cloud_tracks.size())return false;
    if(!cloud_open(cloud_tracks[index].id)){inform(L"无法打开网易云，请检查客户端安装");return false;}
    cloud_index=index;requested_track=cloud_tracks[index];requested_at=GetTickCount64();cloud_request_pending=true;
    return true;
}

static LRESULT CALLBACK messages(int code,WPARAM removed,LPARAM data) {
    if(code==HC_ACTION&&removed==PM_REMOVE) {
        MSG *m=(MSG *)data;
        if(m->message==control_message&&m->wParam==0x50524144) {
            int result=0;
            if(m->lParam==4||m->lParam==5){
                bool force=InterlockedExchange(&info_force,0)!=0||m->lParam==5;
                if(GetCurrentThreadId()==(DWORD)game_thread&&session_active)native_info_refresh(force);
                if(GetCurrentThreadId()==(DWORD)game_thread&&session_active)native_ui_refresh();
                InterlockedExchange(&info_inflight,0);
            }else if(m->lParam==10){
                if(GetCurrentThreadId()==(DWORD)game_thread&&session_active)bgm_sync(radio_native_playing());
                InterlockedExchange(&bgm_inflight,0);
            }else{
                if(GetCurrentThreadId()==(DWORD)game_thread&&session_active)result=radio_control((int)m->lParam);
                InterlockedExchange(&ack,result ? (LONG)m->lParam : -1);
            }
            m->message=WM_NULL;
        }
    }
    return CallNextHookEx(message_hook,code,removed,data);
}
extern "C" void controls_capture(void) {
    DWORD current=GetCurrentThreadId();
    LONG previous=InterlockedCompareExchange(&game_thread,(LONG)current,0);
    if(previous && previous!=(LONG)current)return;
    if(!message_hook) {
        control_message=RegisterWindowMessageW(L"PortableRadio.Native.Controls.20261103");
        message_hook=SetWindowsHookExW(WH_GETMESSAGE,messages,NULL,current);
        if(!message_hook) { trace("Game-thread message hook unavailable");return; }
        char line[96];sprintf_s(line,"Controls attached to observed game thread=%lu",current);trace(line);
    }
    InterlockedExchange(&session_active,1);
}
extern "C" int controls_post(int command) {
    if(command==10){
        if(!game_thread||!message_hook||!session_active||InterlockedCompareExchange(&bgm_inflight,1,0))return 0;
        if(PostThreadMessageW((DWORD)game_thread,control_message,0x50524144,command))return 1;
        InterlockedExchange(&bgm_inflight,0);return 0;
    }
    if(command==4||command==5){
        if(command==5)InterlockedExchange(&info_force,1);
        if(!game_thread||!message_hook||!session_active||InterlockedCompareExchange(&info_inflight,1,0))return 0;
        if(PostThreadMessageW((DWORD)game_thread,control_message,0x50524144,command))return 1;
        InterlockedExchange(&info_inflight,0);return 0;
    }
    InterlockedExchange(&ack,0);
    if(!game_thread||!message_hook||!session_active) { trace("Controls need a motorbike enter/leave event first");return 0; }
    return PostThreadMessageW((DWORD)game_thread,control_message,0x50524144,command)!=0;
}
extern "C" DWORD controls_game_thread(void) { return (DWORD)game_thread; }
extern "C" void controls_session_end(void) {
    bgm_request(0,0);bgm_release(0);
    InterlockedExchange(&session_active,0);InterlockedExchange(&session_end,1);
    InterlockedExchange(&info_force,0);
    native_info_reset();
    native_ui_reset();
}

class Events final: public IMFPMediaPlayerCallback {
    volatile LONG refs=1;
public:
    volatile LONG ready=0, ended=0, error=0, played=0;
    volatile LONG generation=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void **out) override {
        if(!out)return E_POINTER;
        if(id==__uuidof(IUnknown)||id==__uuidof(IMFPMediaPlayerCallback)){*out=this;AddRef();return S_OK;}
        *out=nullptr;return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return (ULONG)InterlockedIncrement(&refs); }
    ULONG STDMETHODCALLTYPE Release() override { LONG n=InterlockedDecrement(&refs);if(!n)delete this;return (ULONG)n; }
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *event) override {
        if(FAILED(event->hrEvent)) { InterlockedExchange(&error,1);return; }
        IMFPMediaItem *item=nullptr;
        if(event->eEventType==MFP_EVENT_TYPE_MEDIAITEM_SET)item=MFP_GET_MEDIAITEM_SET_EVENT(event)->pMediaItem;
        if(event->eEventType==MFP_EVENT_TYPE_PLAYBACK_ENDED)item=MFP_GET_PLAYBACK_ENDED_EVENT(event)->pMediaItem;
        if(item) { DWORD_PTR id=0;item->GetUserData(&id);if(id!=(DWORD_PTR)generation)return; }
        if(event->eEventType==MFP_EVENT_TYPE_MEDIAITEM_SET)InterlockedExchange(&ready,1);
        if(event->eEventType==MFP_EVENT_TYPE_PLAYBACK_ENDED)InterlockedExchange(&ended,1);
        if(event->eEventType==MFP_EVENT_TYPE_PLAY)InterlockedExchange(&played,1);
    }
};

struct Player {
    IMFPMediaPlayer *media=nullptr;
    Events *events=nullptr;
    std::vector<std::wstring> files;
    std::wstring folder,current;
    size_t index=0;
    bool local=false,paused=false,pending=false;
    size_t failures=0;
    float volume=0.5f;
    void scan() {
        files.clear();WIN32_FIND_DATAW info;
        HANDLE search=FindFirstFileW((folder+L"\\*").c_str(),&info);
        if(search!=INVALID_HANDLE_VALUE) {
            do {
                if(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))continue;
                const wchar_t *ext=wcsrchr(info.cFileName,L'.');
                if(ext&&(!_wcsicmp(ext,L".mp3")||!_wcsicmp(ext,L".wav")))files.emplace_back(folder+L"\\"+info.cFileName);
            }while(FindNextFileW(search,&info));FindClose(search);
        }
        std::sort(files.begin(),files.end(),[](const std::wstring&a,const std::wstring&b){return _wcsicmp(a.c_str(),b.c_str())<0;});
        char line[96];sprintf_s(line,"Local music scan: %zu MP3/WAV files",files.size());trace(line);
    }
    void stop() {
        ++events->generation;
        if(media){media->Stop();media->ClearMediaItem();}
        local=false;pending=false;
        InterlockedExchange(&events->ready,0);InterlockedExchange(&events->ended,0);InterlockedExchange(&events->error,0);
    }
    bool load(size_t requested) {
        if(files.empty())return false;
        for(size_t tried=0;tried<files.size();++tried) {
            index=(requested+tried)%files.size();current=files[index];
            LONG generation=InterlockedIncrement(&events->generation);
            InterlockedExchange(&events->ready,0);InterlockedExchange(&events->ended,0);InterlockedExchange(&events->error,0);
            media->Stop();IMFPMediaItem *item=nullptr;
            HRESULT hr=media->CreateMediaItemFromURL(current.c_str(),TRUE,(DWORD_PTR)generation,&item);
            if(SUCCEEDED(hr)) { hr=media->SetMediaItem(item);item->Release(); }
            if(SUCCEEDED(hr)){local=true;pending=true;trace("Local track loading");return true;}
            char line[96];sprintf_s(line,"Local track rejected: HRESULT=0x%08lx",(unsigned long)hr);trace(line);
        }
        trace("No playable local track; returning to native source");stop();controls_post(3);return false;
    }
    void next(int direction=1) {
        std::wstring prior=current;scan();
        auto found=std::find(files.begin(),files.end(),prior);
        size_t next_index=found==files.end()||files.empty()?0:((size_t)(found-files.begin())+files.size()+(direction>0?1:files.size()-1))%files.size();
        failures=0;load(next_index);
    }
    void poll() {
        if(!local)return;
        if(InterlockedExchange(&events->played,0))failures=0;
        if(InterlockedExchange(&events->error,0)) {
            if(++failures>=files.size()){stop();controls_post(3);trace("Local playback failed; restored native source request");}
            else load(index+1);
            return;
        }
        if(InterlockedExchange(&events->ready,0)) {
            pending=false;media->SetVolume(volume);
            if(!paused)media->Play();
            trace(paused?"Local track ready (paused)":"Local playback started");
        }
        if(InterlockedExchange(&events->ended,0)){paused=false;load(index+1);}
    }
};

static DWORD WINAPI run(void *) {
    HRESULT com=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(com)){trace("Local audio COM initialization failed");return 1;}
    if(FAILED(MFStartup(MF_VERSION))) { CoUninitialize();trace("Media Foundation unavailable");return 1; }
    Player p;p.events=new Events;
    wchar_t folder[32768];GetPrivateProfileStringW(L"Music",L"Folder",L"",folder,32768,config_path);p.folder=folder;
    int volume=(int)GetPrivateProfileIntW(L"Music",L"Volume",50,config_path);p.volume=(float)std::max(0,std::min(100,volume))/100.0f;
    HRESULT hr=MFPCreateMediaPlayer(NULL,FALSE,MFP_OPTION_FREE_THREADED_CALLBACK,p.events,NULL,&p.media);
    bool audio_ready=SUCCEEDED(hr);
    if(!audio_ready)trace("Local playback unavailable; native radio controls remain active");
    wchar_t source[40];GetPrivateProfileStringW(L"Music",L"Source",L"NetEase",source,40,config_path);
    bool prefer_cloud=_wcsicmp(source,L"Local")!=0, cloud_mode=false;
    bool suppress_bgm=GetPrivateProfileIntW(L"Audio",L"SuppressBackgroundMusic",1,config_path)!=0;
    int keys[6]={read_key(L"Pause",L"NONE",0),read_key(L"Source",L"NONE",0),read_key(L"Next",L"=",VK_OEM_PLUS),read_key(L"Import",L"NONE",0),read_key(L"Open",L"OEM_3",VK_OEM_3),read_key(L"Previous",L"-",VK_OEM_MINUS)};
    bool held[6]={};int pending_switch=0;ULONGLONG pending_since=0,next_info=0;
    ULONGLONG library_check=0,library_stamp=0,next_bgm=0;
    UINT ui_message=RegisterWindowMessageW(L"PortableRadio.UI.Commands.v2.4");
    MSG ui;PeekMessageW(&ui,NULL,0,0,PM_NOREMOVE);
    cloud_watch_start();
    trace("Portable radio v2.4 ready: minus previous, equals next, OEM_3 official view; legacy F keys disabled");
    while(!stop_worker) {
        bool reveal=false;
        if(InterlockedExchange(&session_end,0)) {if(audio_ready)p.stop();if(cloud_mode)cloud_command(2);cloud_mode=false;cloud_request_pending=false;pending_switch=0;InterlockedExchange(&ack,0);}
        LONG reply=InterlockedExchange(&ack,0);
        if(pending_switch&&reply) {
            if(reply==2){
                if(pending_switch==2){cloud_mode=request_track(0);if(cloud_mode)trace("NetEase client playback requested");else{trace("NetEase URI launch failed");controls_post(3);}}
                else {p.paused=false;p.failures=0;p.load(0);}
            }else inform(L"切换失败：游戏内置电台未响应");
            pending_switch=0;
        }
        if(pending_switch&&GetTickCount64()-pending_since>2500){pending_switch=0;controls_post(3);inform(L"切换超时，请先上、下摩托一次");trace("Game-thread control timed out; imported playback was not started");}
        DWORD foreground_pid=0;HWND foreground=GetForegroundWindow();if(foreground)GetWindowThreadProcessId(foreground,&foreground_pid);
        if(cloud_mode&&GetTickCount64()>=library_check){
            library_check=GetTickCount64()+1000;WIN32_FILE_ATTRIBUTE_DATA attributes;
            if(GetFileAttributesExW(cloud_list_path,GetFileExInfoStandard,&attributes)){
                ULARGE_INTEGER time;time.LowPart=attributes.ftLastWriteTime.dwLowDateTime;time.HighPart=attributes.ftLastWriteTime.dwHighDateTime;
                if(time.QuadPart!=library_stamp){uint64_t prior=cloud_index<cloud_tracks.size()?cloud_tracks[cloud_index].id:0;load_cloud_list();
                    for(size_t j=0;j<cloud_tracks.size();++j)if(cloud_tracks[j].id==prior){cloud_index=j;break;}library_stamp=time.QuadPart;}
            }
        }
        int ui_action=0;
        while(PeekMessageW(&ui,NULL,ui_message,ui_message,PM_REMOVE))
            if(ui.wParam==status_hud_pid()&&(foreground_pid==GetCurrentProcessId()||foreground_pid==status_hud_pid())&&ui.lParam>=1&&ui.lParam<=9)ui_action=(int)ui.lParam;
        if(ui_action==9){
            suppress_bgm=!suppress_bgm;
            if(!WritePrivateProfileStringW(L"Audio",L"SuppressBackgroundMusic",suppress_bgm?L"1":L"0",config_path))trace("BGM option changed for this session; configuration could not be saved");
            trace(suppress_bgm?"Automatic external BGM suppression enabled":"Automatic external BGM suppression disabled");ui_action=0;
        }
        for(int i=0;i<6;++i) {
            bool down=keys[i]>0&&keys[i]<256&&(GetAsyncKeyState(keys[i])&0x8000)!=0;
            bool plain=!(GetAsyncKeyState(VK_CONTROL)&0x8000)&&!(GetAsyncKeyState(VK_MENU)&0x8000)&&!(GetAsyncKeyState(VK_SHIFT)&0x8000);
            bool pressed=down&&!held[i]&&plain&&foreground_pid==GetCurrentProcessId();held[i]=down;
            if(ui_action){int action_index=ui_action==1?0:ui_action==2?5:ui_action==3?2:ui_action==4?4:ui_action==5?3:1;
                if(i==action_index)pressed=true;}
            if(!pressed)continue;
            reveal=true;
            notice_until=0;
            if(pending_switch){inform(L"正在切换音源，请稍候");continue;}
            if(i==0) {
                if(cloud_mode){bool ok=cloud_command(1)!=0;trace(ok?"NetEase pause/resume accepted":"NetEase media session unavailable; enable SMTC");if(!ok)inform(L"无法控制网易云，请在客户端开启 SMTC");}
                else if(p.local) {p.paused=!p.paused;if(!p.pending){if(p.paused)p.media->Pause();else p.media->Play();}trace(p.paused?"Local paused":"Local resumed");}
                else {if(controls_post(1))inform(L"已请求暂停／继续内置电台");else inform(L"请先上、下摩托一次，接入内置电台");}
            } else if(i==1) {
                if(ui_action==6&&cloud_mode){load_cloud_list();request_track(0);continue;}
                if(ui_action==7&&!cloud_mode&&!p.local)continue;
                if((ui_action==6||ui_action==8)&&(cloud_mode||p.local)){if(cloud_mode)cloud_command(2);if(p.local)p.stop();cloud_mode=false;}
                if(ui_action==6)prefer_cloud=true;else if(ui_action==8)prefer_cloud=false;
                if(cloud_mode){cloud_command(2);cloud_mode=false;cloud_request_pending=false;controls_post(3);}
                else if(p.local){p.stop();controls_post(3);}
                else if(prefer_cloud){if(!load_cloud_list())inform(L"尚未导入歌曲，请打开链接导入窗口");else if(controls_post(2)){pending_switch=2;pending_since=GetTickCount64();}else inform(L"请先上、下摩托一次，接入电台控制");}
                else if(audio_ready){p.scan();if(p.files.empty())inform(L"本地音乐文件夹中没有 MP3/WAV");else if(controls_post(2)){pending_switch=1;pending_since=GetTickCount64();}else inform(L"请先上、下摩托一次，接入电台控制");}
            } else if(i==2||i==5){
                if(cloud_mode&&!cloud_tracks.empty()){request_track((cloud_index+cloud_tracks.size()+(i==2?1:cloud_tracks.size()-1))%cloud_tracks.size());trace("Imported NetEase track requested");}
                else if(audio_ready&&p.local)p.next(i==2?1:-1);
                else if(!controls_post(i==2?7:6))inform(L"请先上、下摩托一次，接入官方电台");
            } else if(i==3){std::wstring args=L"--library \""+std::wstring(cloud_list_path)+L"\"";if((INT_PTR)ShellExecuteW(NULL,L"open",import_path,args.c_str(),NULL,SW_SHOWNORMAL)<=32)inform(L"导入窗口启动失败，请检查导入器文件");else trace("NetEase import window opened");}
            else if(i==4){if(cloud_mode){cloud_command(2);cloud_mode=false;cloud_request_pending=false;controls_post(3);}if(p.local){p.stop();controls_post(3);}if(!controls_post(8))inform(L"请先上、下摩托一次，再打开官方电台");}
        }
        if(audio_ready)p.poll();
        if(!pending_switch&&foreground_pid==GetCurrentProcessId()&&GetTickCount64()>=next_info){controls_post(4);next_info=GetTickCount64()+700;}
        if(!pending_switch&&(cloud_mode||p.local)&&native_ui_visible()){
            NativeRadioInfo info=native_info_snapshot();
            if(info.active&&info.playing){if(cloud_mode)cloud_command(2);if(p.local)p.stop();cloud_mode=false;cloud_request_pending=false;trace("Official view selected native playback; external source paused");}
        }
        CloudStatus actual_cloud=cloud_mode?cloud_status():CloudStatus{};
        MFP_MEDIAPLAYER_STATE local_state=MFP_MEDIAPLAYER_STATE_EMPTY;
        bool actual_local=p.local&&audio_ready&&SUCCEEDED(p.media->GetState(&local_state))&&local_state==MFP_MEDIAPLAYER_STATE_PLAYING;
        bgm_request(suppress_bgm,session_active&&(cloud_mode?(actual_cloud.available&&actual_cloud.playback==4):actual_local));
        if(bgm_available()&&GetTickCount64()>=next_bgm){controls_post(10);next_bgm=GetTickCount64()+250;}
        RadioStatus display;
        if(cloud_mode) {
            auto info=describe_cloud(actual_cloud,cloud_tracks,cloud_index,requested_track,cloud_request_pending,
                                     GetTickCount64()-requested_at>=12000);
            display=info.card;cloud_index=info.index;cloud_request_pending=info.pending;
        }else if(p.local){
            display.source=L"本地音乐";auto slash=p.current.find_last_of(L'\\');display.title=p.current.substr(slash==std::wstring::npos?0:slash+1);
            display.state=p.pending?L"载入中":p.paused?L"已暂停":L"播放中";
            display.detail=std::to_wstring(p.index+1)+L" / "+std::to_wstring(p.files.size());
        }else {
            NativeRadioInfo native=native_info_snapshot();
            display.source=L"游戏内置电台";
            display.title=native.title[0]?native.title:native.active?L"曲目 #"+std::to_wstring(native.track_id):L"摩托电台";
            display.state=!native.observed?L"等待读取":!native.active?L"未播放":native.playing?L"播放中":L"已暂停";
            display.detail=!native.supported?L"曲名接口不匹配，请查看诊断日志":!native.observed?L"先上、下摩托一次，读取当前曲名":
                native.active&&!native.title[0]?L"当前曲名未解析，可按查看键重试":L"内置曲目 · 支持下车继续播放";
        }
        if(pending_switch){display.state=L"切换音源中";display.detail=L"正在等待游戏线程确认";}
        if(GetTickCount64()<notice_until)display.detail=notice;
        display.view_open=native_ui_visible()!=0;
        display.riding=radio_riding()!=0;
        display.bgm_enabled=suppress_bgm;display.bgm_available=bgm_available()!=0;display.bgm_active=bgm_active()!=0;
        status_publish(display,reveal);
        Sleep(20);
    }
    cloud_watch_stop();status_stop();
    if(audio_ready){p.stop();p.media->Shutdown();p.media->Release();}
    p.events->Release();MFShutdown();CoUninitialize();return 0;
}
extern "C" void controls_start(HMODULE module,const wchar_t *config,RadioLog log) {
    logger=log;wcscpy_s(config_path,config);wcscpy_s(cloud_list_path,config);
    wchar_t *slash=wcsrchr(cloud_list_path,L'\\');if(slash)*slash=0;wcscat_s(cloud_list_path,L"\\PortableRadioNetEase.ini");
    GetModuleFileNameW(module,import_path,32768);slash=wcsrchr(import_path,L'\\');if(slash)*slash=0;wcscat_s(import_path,L"\\NetEaseImporter.exe");
    status_start(module,config);
    worker=CreateThread(NULL,0,run,NULL,0,NULL);
}
extern "C" void controls_shutdown(void) {
    bgm_request(0,0);bgm_release(radio_native_playing());
    InterlockedExchange(&stop_worker,1);
    if(worker){WaitForSingleObject(worker,5000);CloseHandle(worker);worker=NULL;}
    if(message_hook){UnhookWindowsHookEx(message_hook);message_hook=NULL;}
}
#ifdef RADIO_AUDIO_LAB
extern "C" int radio_control(int) { return 1; }
static bool await_state(Player &p,MFP_MEDIAPLAYER_STATE expected) {
    for(int i=0;i<300;++i){p.poll();MFP_MEDIAPLAYER_STATE state;p.media->GetState(&state);if(state==expected&&!p.pending)return true;Sleep(10);}return false;
}
int wmain(int argc,wchar_t **argv) {
    if(argc!=2)return 2;
    logger=[](const char *s){puts(s);};
    if(FAILED(CoInitializeEx(NULL,COINIT_MULTITHREADED))||FAILED(MFStartup(MF_VERSION)))return 3;
    Player p;p.events=new Events;p.folder=argv[1];p.volume=0;
    HRESULT hr=MFPCreateMediaPlayer(NULL,FALSE,MFP_OPTION_FREE_THREADED_CALLBACK,p.events,NULL,&p.media);
    if(FAILED(hr)){printf("create_failed=0x%08lx\n",(unsigned long)hr);return 4;}
    p.media->SetVolume(0);p.scan();if(p.files.size()!=3)return 5;
    if(!p.load(0)||!await_state(p,MFP_MEDIAPLAYER_STATE_PLAYING))return 6;
    if(p.index!=1)return 7;
    p.media->Pause();p.paused=true;if(!await_state(p,MFP_MEDIAPLAYER_STATE_PAUSED))return 8;
    Sleep(100);p.media->Play();p.paused=false;if(!await_state(p,MFP_MEDIAPLAYER_STATE_PLAYING))return 9;
    if(!p.load(2)||!await_state(p,MFP_MEDIAPLAYER_STATE_PLAYING))return 10;
    bool advanced=false;
    for(int i=0;i<500;++i){p.poll();if(p.index==1&&!p.pending){advanced=true;break;}Sleep(10);}
    if(!advanced)return 11;
    p.stop();p.media->Shutdown();p.media->Release();p.events->Release();MFShutdown();CoUninitialize();
    puts("PASS: Unicode MP3/WAV folder, corrupt-file skip, real decode/play, pause/resume, automatic next, stop/shutdown (muted)");return 0;
}
#endif
