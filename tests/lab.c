#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "bridge.h"
#include "controls.h"
#include "native_info.h"
#include "native_ui.h"
#include <wchar.h>
typedef void *(*Ptr0)(void);
typedef void *(*Ptr1)(void *);
typedef void (*Void1)(void *);
typedef void (*SetPath)(const char *);
typedef void *(*Init)(const char *, const char *);
typedef void *(*Open)(void *, const char *);
typedef void *(*Class)(void *, const char *, const char *);
typedef void *(*Method)(void *, const char *, int);
typedef void *(*Field)(void *, const char *);
typedef const char *(*String)(void *);
typedef uint32_t (*Uint)(void *);
typedef void *(*Invoke)(void *, void *, void **, void **);
typedef void (*FieldValue)(void *, void *, void *);
static HMODULE rt;
static void *domain, *image, *klass, *instance;
static Class get_class;
static Method get_method;
static Field get_field;
static FieldValue set_value, read_value;
static Invoke invoke;
static int prepare_result, enable_result;
static const char *guid;
static uint32_t token;
static void trace(const char *s) { puts(s); fflush(stdout); }
static DWORD WINAPI prepare_worker(void *unused) {
    (void)unused;
    Ptr1 attach = (Ptr1)GetProcAddress(rt,"mono_thread_attach");
    Void1 detach = (Void1)GetProcAddress(rt,"mono_thread_detach");
    void *thread = attach(domain);
    prepare_result = radio_prepare(rt,domain,image,guid,token,trace);
    detach(thread);
    /* Enable only after leaving Mono, so this thread does not block a Mono GC. */
    enable_result = prepare_result ? -1 : radio_enable();
    return 0;
}
static void set_int(const char *name, int32_t n) { set_value(instance,get_field(klass,name),&n); }
static void set_bool(const char *name, int n) { unsigned char b=(unsigned char)n; set_value(instance,get_field(klass,name),&b); }
static int get_int(const char *name) { int32_t n=0;read_value(instance,get_field(klass,name),&n);return n; }
static int get_bool(const char *name) { unsigned char b=0;read_value(instance,get_field(klass,name),&b);return b; }
static void call(const char *name, void **args) {
    void *exception=NULL;
    invoke(get_method(klass,name,args ? 2 : 0),instance,args,&exception);
    if(exception) { puts("fixture_exception"); ExitProcess(30); }
}
static void reset(int handle, int playing, int vehicle) {
    set_int("CurHandleId",handle);set_bool("IsPlaying",playing);set_bool("IsInVehicle",vehicle);
    set_int("Pauses",0);set_int("NextTracks",0);set_int("Stops",0);set_int("SeatChanges",0);
}
static DWORD WINAPI wrong_info_thread(void *unused){(void)unused;native_info_refresh(1);return 0;}
#define CHECK(x) do { if(!(x)) { printf("CHECK FAILED line=%d\n",__LINE__);return 40; } } while(0)
int wmain(int argc,wchar_t **argv) {
    if(argc!=4)return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    rt=LoadLibraryExW(argv[1],NULL,LOAD_WITH_ALTERED_SEARCH_PATH);if(!rt)return 3;
    char path[32768];WideCharToMultiByte(CP_UTF8,0,argv[2],-1,path,32768,NULL,NULL);
    ((SetPath)GetProcAddress(rt,"mono_set_assemblies_path"))(path);
    domain=((Init)GetProcAddress(rt,"mono_jit_init_version"))("PortableRadioBehaviourLab","v4.0.30319");
    WideCharToMultiByte(CP_UTF8,0,argv[3],-1,path,32768,NULL,NULL);
    void *assembly=((Open)GetProcAddress(rt,"mono_domain_assembly_open"))(domain,path);if(!assembly)return 4;
    image=((Ptr1)GetProcAddress(rt,"mono_assembly_get_image"))(assembly);
    guid=((String)GetProcAddress(rt,"mono_image_get_guid"))(image);
    get_class=(Class)GetProcAddress(rt,"mono_class_from_name");get_method=(Method)GetProcAddress(rt,"mono_class_get_method_from_name");
    get_field=(Field)GetProcAddress(rt,"mono_class_get_field_from_name");set_value=(FieldValue)GetProcAddress(rt,"mono_field_set_value");
    read_value=(FieldValue)GetProcAddress(rt,"mono_field_get_value");invoke=(Invoke)GetProcAddress(rt,"mono_runtime_invoke");
    klass=get_class(image,"","MotorcycleMusicPlayerController");
    token=((Uint)GetProcAddress(rt,"mono_method_get_token"))(get_method(klass,"OnLeaveVehicle",2));
    CHECK(radio_prepare(rt,domain,image,"00000000-0000-0000-0000-000000000000",token,trace)==2);
    CHECK(radio_prepare(rt,domain,image,guid,token+1,trace)==4);
    HANDLE worker=CreateThread(NULL,0,prepare_worker,NULL,0,NULL);CHECK(worker!=NULL);
    CHECK(WaitForSingleObject(worker,10000)==WAIT_OBJECT_0);CloseHandle(worker);
    printf("prepare=%d enable=%d\n",prepare_result,enable_result);CHECK(!prepare_result&&!enable_result);
    typedef void *(*NewObject)(void *,void *);
    instance=((NewObject)GetProcAddress(rt,"mono_object_new"))(domain,klass);
    ((Void1)GetProcAddress(rt,"mono_runtime_object_init"))(instance);
    /* Root the synthetic instance across managed/native boundaries. */
    typedef uint32_t (*NewRoot)(void *,int);
    uint32_t root=((NewRoot)GetProcAddress(rt,"mono_gchandle_new"))(instance,1);
    void *passenger=NULL;unsigned char seat_change=0;void *args[]={passenger,&seat_change};
    reset(123,1,1);call("OnLeaveVehicle",args);
    CHECK(get_bool("IsPlaying")&&get_bool("IsInVehicle")&&get_int("CurHandleId")==123&&get_int("Pauses")==0);
    call("OnMusicEnd",NULL);CHECK(get_int("NextTracks")==1);
    reset(0,1,1);call("OnLeaveVehicle",args);CHECK(get_bool("IsPlaying")&&get_int("Pauses")==0);
    reset(-1,1,1);call("OnLeaveVehicle",args);CHECK(get_int("Pauses")==1&&!get_bool("IsInVehicle"));
    reset(123,0,1);call("OnLeaveVehicle",args);CHECK(get_int("Pauses")==0&&!get_bool("IsPlaying")&&get_bool("IsInVehicle"));
    reset(123,1,0);call("OnLeaveVehicle",args);CHECK(get_int("Pauses")==1);
    radio_set_enabled(0);reset(123,1,1);seat_change=1;call("OnLeaveVehicle",args);
    CHECK(get_int("Pauses")==1&&get_int("SeatChanges")==1);radio_set_enabled(1);seat_change=0;
    reset(123,1,1);call("OnLeaveVehicle",args);call("OnBackLoginView",NULL);
    CHECK(get_int("Stops")==1&&!get_bool("IsPlaying")&&get_int("CurHandleId")==-1);
    reset(123,1,1);call("OnLeaveVehicle",args);call("OnClear",NULL);CHECK(get_int("Stops")==1&&!get_bool("IsPlaying"));
    reset(123,1,1);call("OnEnterVehicle",args);
    CHECK(radio_control(1)==1&&!get_bool("IsPlaying"));
    call("OnLeaveVehicle",args);CHECK(!get_bool("IsPlaying")&&get_bool("IsInVehicle"));
    CHECK(radio_control(1)==1&&get_bool("IsPlaying"));
    CHECK(radio_control(2)==1&&!get_bool("IsPlaying"));
    CHECK(radio_control(3)==1&&get_bool("IsPlaying"));
    CHECK(radio_control(1)==1&&!get_bool("IsPlaying"));
    CHECK(radio_control(2)==1);CHECK(radio_control(3)==1&&!get_bool("IsPlaying"));
    /* Queue a pause on the observed thread and pump it exactly once. */
    MSG msg;PeekMessageW(&msg,NULL,0,0,PM_NOREMOVE);
    CHECK(controls_post(1)==1);
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    CHECK(get_bool("IsPlaying"));
    /* Exercise actual Mono nullable boxing, struct getters and localized strings. */
    void *model_class=get_class(image,"","MotorcycleMusicPlayerModel");
    void *exception=NULL;
    void *model=invoke(get_method(model_class,"get_Instance",0),NULL,NULL,&exception);CHECK(model&&!exception);
    uint32_t model_root=((NewRoot)GetProcAddress(rt,"mono_gchandle_new"))(model,1);CHECK(model_root);
    void *id_field=get_field(model_class,"CurPlayMusicId");CHECK(id_field);
    native_info_refresh(1);NativeRadioInfo info=native_info_snapshot();
    CHECK(info.supported&&info.observed&&info.active&&info.playing&&info.track_id==501);
    CHECK(!wcscmp(info.title,L"测试原生曲目甲"));
    int32_t id=502;set_value(model,id_field,&id);
    native_info_refresh(0);info=native_info_snapshot();CHECK(info.track_id==502&&!wcscmp(info.title,L"测试原生曲目乙"));
    CHECK(radio_control(1));native_info_refresh(0);CHECK(!native_info_snapshot().playing);
    CHECK(radio_control(1));
    id=501;set_value(model,id_field,&id);
    HANDLE wrong_thread=CreateThread(NULL,0,wrong_info_thread,NULL,0,NULL);CHECK(wrong_thread);
    CHECK(WaitForSingleObject(wrong_thread,3000)==WAIT_OBJECT_0);CloseHandle(wrong_thread);
    CHECK(native_info_snapshot().track_id==502);native_info_refresh(0);CHECK(native_info_snapshot().track_id==501);
    id=503;set_value(model,id_field,&id);native_info_refresh(1);info=native_info_snapshot();CHECK(!info.title[0]);
    id=999;set_value(model,id_field,&id);native_info_refresh(1);info=native_info_snapshot();CHECK(!info.title[0]);
    id=998;set_value(model,id_field,&id);native_info_refresh(1);info=native_info_snapshot();CHECK(!info.title[0]);
    id=501;set_value(model,id_field,&id);native_info_refresh(1);info=native_info_snapshot();CHECK(!wcscmp(info.title,L"测试原生曲目甲"));
    set_int("CurHandleId",-1);native_info_refresh(0);info=native_info_snapshot();CHECK(!info.active&&!info.title[0]);
    set_int("CurHandleId",123);
    CHECK(controls_post(5));
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    info=native_info_snapshot();CHECK(info.active&&info.title[0]);
    unsigned char alternate=1;
    set_value(model,get_field(model_class,"UseAlternateLanguage"),&alternate);
    native_info_refresh(0);info=native_info_snapshot();CHECK(!wcscmp(info.title,L"测试原生曲目甲"));
    CHECK(controls_post(4));CHECK(!controls_post(5)); // Coalesce a forced refresh with an existing queued refresh.
    while(PeekMessageW(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    info=native_info_snapshot();CHECK(!wcscmp(info.title,L"Native track A"));
    alternate=0;set_value(model,get_field(model_class,"UseAlternateLanguage"),&alternate);
    controls_session_end();info=native_info_snapshot();CHECK(!info.observed&&!info.title[0]);
    call("OnEnterVehicle",args);
    CHECK(radio_control(6));CHECK(get_int("PreviousRequests")==1);CHECK(native_info_snapshot().track_id==504);
    CHECK(radio_control(7));CHECK(get_int("NextRequests")==1);CHECK(native_info_snapshot().track_id==501);
    CHECK(radio_control(8));native_ui_refresh();CHECK(get_int("OpenedViews")==1&&native_ui_visible());
    controls_session_end();CHECK(!native_ui_visible());call("OnEnterVehicle",args);
    puts("PASS: native localized names, real nullable boxing, automatic track changes, pause state, wrong-thread rejection, missing data/exceptions, recovery, message dispatch and session reset");
    /* Stress both ABI paths with changing fields. */
    for(int i=0;i<1000;++i){reset(i,1,1);call("OnLeaveVehicle",args);CHECK(get_int("Pauses")==0);}
    CHECK(radio_remove()==0);reset(123,1,1);call("OnLeaveVehicle",args);CHECK(get_int("Pauses")==1);
    controls_shutdown();
    typedef void (*FreeRoot)(uint32_t);((FreeRoot)GetProcAddress(rt,"mono_gchandle_free"))(model_root);((FreeRoot)GetProcAddress(rt,"mono_gchandle_free"))(root);
    printf("PASS: version guards, active/zero/invalid handles, paused/inactive/disabled pass-through, track continuation, logout/clear cleanup, 1000 calls, hook removal; skipped=%ld\n",radio_skipped());
    return 0;
}
