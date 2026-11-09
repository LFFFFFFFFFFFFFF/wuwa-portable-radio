/* Exact-version radio detours; managed playback commands run on the observed game thread. */
#include "bridge.h"
#include "controls.h"
#include "native_info.h"
#include "native_ui.h"
#include "MinHook.h"
#include <string.h>
#include <stdio.h>

typedef void *(*Ptr1)(void *);
typedef void *(*ClassByName)(void *, const char *, const char *);
typedef void *(*MethodByName)(void *, const char *, int);
typedef void *(*FieldByName)(void *, const char *);
typedef const char *(*String1)(void *);
typedef uint32_t (*Uint1)(void *);
typedef uint32_t (*MethodFlags)(void *, uint32_t *);
typedef int (*Int1)(void *);
typedef void *(*ParamType)(void *, void **);
typedef void (*ReadField)(void *, void *, void *);
typedef void *(*FindJit)(void *, void *);
typedef void (*LeaveFn)(void *, void *, int);

static ReadField read_field;
static void *handle_field, *playing_field, *vehicle_field, *target;
static LeaveFn original_leave;
static volatile LONG enabled = 1, skips;
static int hook_created, hook_enabled;
static RadioLog logger;
typedef void (*ObjectFn)(void *);
typedef unsigned char (*ClearFn)(void *);
typedef void *(*InvokeFn)(void *,void *,void **,void **);
static LeaveFn original_enter;
static ObjectFn original_login;
static ClearFn original_clear;
static void *extra_targets[3], *pause_method, *resume_method, *instance_method;
static InvokeFn invoke;
static int native_restore;
static void *quick_method,*open_method,*check_method;
static Ptr1 unbox_result;
static int navigation_ready;
static volatile LONG riding;
int radio_riding(void){return (int)InterlockedCompareExchange(&riding,0,0);}
static void enter_detour(void *self,void *passenger,int seat_change) { InterlockedExchange(&riding,1);controls_capture();original_enter(self,passenger,seat_change); }
static void login_detour(void *self) { InterlockedExchange(&riding,0);native_restore=0;controls_session_end();original_login(self); }
static unsigned char clear_detour(void *self) { InterlockedExchange(&riding,0);native_restore=0;controls_session_end();return original_clear(self); }

static void trace(const char *s) { if (logger) logger(s); }

static void leave_detour(void *self, void *passenger, int is_seat_change) {
    if(!is_seat_change)InterlockedExchange(&riding,0);
    controls_capture();
    if (self && InterlockedCompareExchange(&enabled, 0, 0)) {
        int32_t handle = -1;
        unsigned char playing = 0, in_vehicle = 0;
        read_field(self, handle_field, &handle);
        read_field(self, playing_field, &playing);
        read_field(self, vehicle_field, &in_vehicle);
        if (in_vehicle && handle != -1) {
            LONG count = InterlockedIncrement(&skips);
            if (count <= 8) {
                char line[128];
                sprintf_s(line, sizeof(line), "Retain radio session: handle=%ld playing=%u dismount=%ld", (long)handle,playing,count);
                trace(line);
            }
            return;
        }
    }
    original_leave(self, passenger, is_seat_change);
}

int radio_prepare(HMODULE runtime, void *domain, void *image, const char *guid, uint32_t token, RadioLog log) {
    if (target || hook_created) return 20;
    logger = log;
#define API(type,var,name) type var = (type)GetProcAddress(runtime,name); if (!var) { trace("Required Mono export missing"); return 1; }
    API(String1, image_guid, "mono_image_get_guid");
    API(ClassByName, find_class, "mono_class_from_name");
    API(MethodByName, find_method, "mono_class_get_method_from_name");
    API(FieldByName, find_field, "mono_class_get_field_from_name");
    API(Ptr1, field_type, "mono_field_get_type");
    API(Int1, type_kind, "mono_type_get_type");
    API(Uint1, field_flags, "mono_field_get_flags");
    API(Uint1, method_token, "mono_method_get_token");
    API(MethodFlags, method_flags, "mono_method_get_flags");
    API(Ptr1, method_signature, "mono_method_signature");
    API(Uint1, param_count, "mono_signature_get_param_count");
    API(Ptr1, return_type, "mono_signature_get_return_type");
    API(ParamType, param_type, "mono_signature_get_params");
    API(Ptr1, class_from_type, "mono_class_from_mono_type");
    API(String1, class_name, "mono_class_get_name");
    API(String1, class_namespace, "mono_class_get_namespace");
    API(Ptr1, compile, "mono_compile_method");
    API(FindJit, find_jit, "mono_jit_info_table_find");
    API(Ptr1, jit_method, "mono_jit_info_get_method");
    API(Ptr1, jit_start, "mono_jit_info_get_code_start");
    API(Int1, jit_size, "mono_jit_info_get_code_size");
    read_field = (ReadField)GetProcAddress(runtime, "mono_field_get_value");
    if (!read_field) return 1;
#undef API
    const char *actual = image_guid(image);
    if (!actual || !guid || _stricmp(actual, guid)) { trace("Assembly MVID mismatch; no hook installed"); return 2; }
    void *klass = find_class(image, "", "MotorcycleMusicPlayerController");
    if (!klass) return 3;
    void *method = find_method(klass, "OnLeaveVehicle", 2);
    if (!method || method_token(method) != token || (method_flags(method, NULL) & 0x10)) return 4;
    void *sig = method_signature(method);
    if (!sig || param_count(sig) != 2 || type_kind(return_type(sig)) != 1) return 5;
    void *iter = NULL;
    void *vehicle_type = param_type(sig, &iter);
    void *boolean_type = param_type(sig, &iter);
    if (!vehicle_type || !boolean_type || type_kind(vehicle_type) != 18 || type_kind(boolean_type) != 2) return 6;
    void *vehicle_class = class_from_type(vehicle_type);
    if (!vehicle_class || strcmp(class_name(vehicle_class), "VehiclePassengerInfo") || strcmp(class_namespace(vehicle_class), "")) return 6;
    handle_field = find_field(klass, "CurHandleId");
    playing_field = find_field(klass, "IsPlaying");
    vehicle_field = find_field(klass, "IsInVehicle");
    if (!handle_field || !playing_field || !vehicle_field) return 7;
    if ((field_flags(handle_field) | field_flags(playing_field) | field_flags(vehicle_field)) & 0x10) return 7;
    if (type_kind(field_type(handle_field)) != 8 || type_kind(field_type(playing_field)) != 2 || type_kind(field_type(vehicle_field)) != 2) return 8;
    if (!find_method(klass, "OnBackLoginView", 0) || !find_method(klass, "OnClear", 0) || !find_method(klass, "StopMusic", 0)) return 9;
    pause_method=find_method(klass,"PauseMusic",0);resume_method=find_method(klass,"ResumeMusic",0);
    instance_method=find_method(klass,"get_Instance",0);
    invoke=(InvokeFn)GetProcAddress(runtime,"mono_runtime_invoke");
    if(!pause_method||!resume_method||!instance_method||!invoke)return 13;
    if(type_kind(return_type(method_signature(pause_method)))!=1||type_kind(return_type(method_signature(resume_method)))!=1||
       !(method_flags(instance_method,NULL)&0x10))return 13;
    void *others[3]={find_method(klass,"OnEnterVehicle",2),find_method(klass,"OnBackLoginView",0),find_method(klass,"OnClear",0)};
    if(!others[0])return 14;
    for(int i=0;i<3;++i) {
        void *body=compile(others[i]);void *info=body?find_jit(domain,body):NULL;
        if(!info||jit_method(info)!=others[i]||jit_start(info)!=body||jit_size(info)<14)return 15;
        extra_targets[i]=body;
    }
    void *code = compile(method);
    if (!code) return 10;
    void *ji = find_jit(domain, code);
    if (!ji || jit_method(ji) != method || jit_start(ji) != code || jit_size(ji) < 14) {
        trace("Target is not a unique verified JIT method body; no hook installed");
        return 11;
    }
    const unsigned char *bytes = code;
    if (bytes[0] == 0xe9 || (bytes[0] == 0xff && bytes[1] == 0x25)) {
        trace("Target entry already redirects; no hook installed"); return 12;
    }
    target = code;
    int title_result=native_info_prepare(runtime,image,logger);
    if(title_result){char line[96];sprintf_s(line,sizeof(line),"Native title reader unavailable: contract stage=%d; playback hooks unaffected",title_result);trace(line);}
    quick_method=find_method(klass,"QuickPlayMusic",1);open_method=find_method(klass,"OpenMusicPlayerView",0);check_method=find_method(klass,"CheckIsEnable",1);
    unbox_result=(Ptr1)GetProcAddress(runtime,"mono_object_unbox");
    if(quick_method&&open_method&&check_method&&unbox_result){
        void *qs=method_signature(quick_method),*os=method_signature(open_method),*cs=method_signature(check_method),*qi=NULL,*ci=NULL;
        navigation_ready=qs&&os&&cs&&!(method_flags(quick_method,NULL)&0x10)&&!(method_flags(open_method,NULL)&0x10)&&!(method_flags(check_method,NULL)&0x10)&&
            param_count(qs)==1&&type_kind(return_type(qs))==1&&type_kind(param_type(qs,&qi))==2&&
            param_count(os)==0&&type_kind(return_type(os))==1&&param_count(cs)==1&&type_kind(return_type(cs))==2&&type_kind(param_type(cs,&ci))==2;
    }
    trace(navigation_ready?"Official radio navigation ready: QuickPlayMusic(false/true), OpenMusicPlayerView":"Official radio navigation unavailable; retained playback remains active");
    int ui_result=native_ui_prepare(runtime,domain,image);
    if(ui_result){char line[96];sprintf_s(line,sizeof(line),"Radio view visibility unavailable: stage=%d",ui_result);trace(line);}
    trace("Exact assembly, method signature, field types and JIT body verified");
    return 0;
}

int radio_enable(void) {
    if (!target || hook_created) return 21;
    MH_STATUS result = MH_Initialize();
    if (result != MH_OK) return 100 + result;
    result = MH_CreateHook(target, (void *)leave_detour, (void **)&original_leave);
    if (result != MH_OK) { MH_Uninitialize(); return 200 + result; }
    hook_created = 1;
    void *detours[3]={(void *)enter_detour,(void *)login_detour,(void *)clear_detour};
    void **originals[3]={(void **)&original_enter,(void **)&original_login,(void **)&original_clear};
    for(int i=0;i<3;++i) {
        result=MH_CreateHook(extra_targets[i],detours[i],originals[i]);
        if(result!=MH_OK){radio_remove();return 250+result;}
    }
    result = MH_EnableHook(MH_ALL_HOOKS);
    if (result != MH_OK) {
        radio_remove();
        return 300 + result;
    }
    hook_enabled = 1;
    trace("Radio dismount hook enabled; existing stream retained when playing");
    return 0;
}

int radio_remove(void) {
    if (hook_enabled) {
        MH_STATUS result = MH_DisableHook(MH_ALL_HOOKS);
        if (result != MH_OK) return 400 + result;
        hook_enabled = 0;
    }
    if (hook_created) {
        for(int i=0;i<3;++i)if(extra_targets[i])MH_RemoveHook(extra_targets[i]);
        MH_STATUS result = MH_RemoveHook(target);
        if (result != MH_OK) return 500 + result;
        hook_created = 0;
        MH_Uninitialize();
    }
    target = NULL;
    return 0;
}
void radio_set_enabled(int value) { InterlockedExchange(&enabled, value != 0); }
LONG radio_skipped(void) { return InterlockedCompareExchange(&skips, 0, 0); }
int radio_native_playing(void) {
    if(GetCurrentThreadId()!=controls_game_thread()||!invoke||!instance_method)return 0;
    void *exception=NULL,*self=invoke(instance_method,NULL,NULL,&exception);if(exception||!self)return 0;
    int32_t stream=-1;unsigned char playing=0;read_field(self,handle_field,&stream);read_field(self,playing_field,&playing);
    return stream!=-1&&playing;
}

int radio_control(int command) {
    if(GetCurrentThreadId()!=controls_game_thread()) {trace("Rejected control on wrong thread");return 0;}
    void *exception=NULL;
    void *self=invoke(instance_method,NULL,NULL,&exception);
    if(exception)return 0;
    if(!self)return command==2||command==3;
    if(command==6||command==7||command==8){
        if(!navigation_ready)return 0;
        unsigned char tips=0;void *check_args[]={&tips};void *enabled_result=invoke(check_method,self,check_args,&exception);
        if(exception||!enabled_result||!*(unsigned char *)unbox_result(enabled_result)){trace("Official radio is not enabled in this scene");return 0;}
        if(command==8){invoke(open_method,self,NULL,&exception);trace(exception?"Official radio view raised an exception":"Official radio view requested");}
        else {unsigned char next=command==7;void *args[]={&next};invoke(quick_method,self,args,&exception);trace(exception?"Official radio navigation raised an exception":next?"Official next track requested":"Official previous track requested");}
        native_info_refresh(1);native_ui_refresh();return exception==NULL;
    }
    int32_t handle=-1;unsigned char playing=0;
    read_field(self,handle_field,&handle);read_field(self,playing_field,&playing);
    if(command==2) {
        if(!playing||handle==-1)return 1;
        native_restore=1;
        invoke(pause_method,self,NULL,&exception);
        read_field(self,playing_field,&playing);
        if(exception||playing){native_restore=0;trace("Native pause not accepted; local source not started");return 0;}
        native_info_refresh(0);trace("Native paused for local source");return 1;
    }
    if(command==3) {
        if(native_restore&&handle!=-1)invoke(resume_method,self,NULL,&exception);
        native_restore=0;native_info_refresh(0);trace("Returned to native source");return exception==NULL;
    }
    if(command==1&&handle!=-1) {
        invoke(playing?pause_method:resume_method,self,NULL,&exception);
        if(exception){trace("Native pause/resume raised an exception");return 0;}
        trace(playing?"Native pause requested":"Native resume requested");return 1;
    }
    trace("No active native radio stream");return 0;
}
