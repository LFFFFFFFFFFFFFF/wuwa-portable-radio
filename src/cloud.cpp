#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <objbase.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <mutex>
#include <chrono>
#include "cloud.h"

static std::mutex snapshot_mutex;
static CloudStatus snapshot;
static ULONGLONG snapshot_time;
static HANDLE watcher;
static volatile LONG stopping;

/* Metadata runs on its own COM thread; a slow client cannot stall hotkeys. */
static DWORD WINAPI watch(void *) {
    using namespace winrt::Windows::Media::Control;
    using namespace std::chrono_literals;
    HRESULT com=CoInitializeEx(NULL,COINIT_MULTITHREADED);
    if(FAILED(com))return 1;
    GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
    while(!stopping) {
        CloudStatus next;
        try {
            if(!manager) {
                auto request=GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
                if(request.wait_for(750ms)==winrt::Windows::Foundation::AsyncStatus::Completed)manager=request.get();
                else request.Cancel();
            }
            if(manager)for(auto const &session:manager.GetSessions()) {
                std::wstring app=session.SourceAppUserModelId().c_str();
                std::transform(app.begin(),app.end(),app.begin(),::towlower);
                if(app.find(L"cloudmusic")==std::wstring::npos&&app.find(L"netease")==std::wstring::npos)continue;
                auto request=session.TryGetMediaPropertiesAsync();
                if(request.wait_for(500ms)==winrt::Windows::Foundation::AsyncStatus::Completed) {
                    auto properties=request.get();
                    next.title=properties.Title().c_str();next.artist=properties.Artist().c_str();
                    auto playback=session.GetPlaybackInfo();
                    next.playback=(int)playback.PlaybackStatus();next.available=true;
                    try{
                        auto rate=playback.PlaybackRate();if(rate&&rate.Value()>=0.25&&rate.Value()<=4.0)next.rate_permille=(int)(rate.Value()*1000);
                        auto timeline=session.GetTimelineProperties();
                        next.position_ms=std::chrono::duration_cast<std::chrono::milliseconds>(timeline.Position()-timeline.StartTime()).count();
                        next.duration_ms=std::chrono::duration_cast<std::chrono::milliseconds>(timeline.EndTime()-timeline.StartTime()).count();
                        auto age=std::chrono::duration_cast<std::chrono::milliseconds>(winrt::clock::now()-timeline.LastUpdatedTime()).count();
                        next.timeline=next.duration_ms>0&&next.duration_ms<=86400000&&next.position_ms>=0&&age>=-1000&&age<=86400000;
                        if(next.timeline&&next.playback==4&&age>0)next.position_ms+=age*next.rate_permille/1000;
                        next.position_ms=std::clamp(next.position_ms,(int64_t)0,std::max((int64_t)0,next.duration_ms));
                        {std::lock_guard<std::mutex> lock(snapshot_mutex);if(next.title!=snapshot.title||next.artist!=snapshot.artist)next.timeline=false;}
                    }catch(winrt::hresult_error const &){next.timeline=false;}
                }else request.Cancel();
                break;
            }
        }catch(winrt::hresult_error const &){manager=nullptr;}
        {
            std::lock_guard<std::mutex> lock(snapshot_mutex);
            snapshot=next;snapshot_time=GetTickCount64();
        }
        for(int i=0;i<10&&!stopping;++i)Sleep(50);
    }
    manager=nullptr;CoUninitialize();return 0;
}
void cloud_watch_start(){InterlockedExchange(&stopping,0);watcher=CreateThread(NULL,0,watch,NULL,0,NULL);}
void cloud_watch_stop(){InterlockedExchange(&stopping,1);if(watcher){WaitForSingleObject(watcher,3000);CloseHandle(watcher);watcher=nullptr;}}
CloudStatus cloud_status(){
    std::lock_guard<std::mutex> lock(snapshot_mutex);ULONGLONG age=GetTickCount64()-snapshot_time;
    if(age>=4000)return CloudStatus{};
    CloudStatus result=snapshot;
    if(result.timeline&&result.playback==4)result.position_ms=std::min(result.duration_ms,result.position_ms+(int64_t)age*result.rate_permille/1000);
    return result;
}
int cloud_command(int command) {
    try {
        auto manager=winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
        for(auto const &session:manager.GetSessions()) {
            std::wstring app=session.SourceAppUserModelId().c_str();
            std::transform(app.begin(),app.end(),app.begin(),::towlower);
            if(app.find(L"cloudmusic")==std::wstring::npos&&app.find(L"netease")==std::wstring::npos)continue;
            if(command==1)return session.TryTogglePlayPauseAsync().get();
            if(command==2)return session.TryPauseAsync().get();
            if(command==3)return session.TryPlayAsync().get();
            if(command==4)return session.TrySkipNextAsync().get();
        }
    } catch(winrt::hresult_error const &) { return 0; }
    return 0;
}
int cloud_open(uint64_t id) {
    if(!id)return 0;
    wchar_t uri[128];swprintf_s(uri,L"orpheus://song/%llu",(unsigned long long)id);
    return (INT_PTR)ShellExecuteW(NULL,L"open",uri,NULL,NULL,SW_SHOWMINNOACTIVE)>32;
}
