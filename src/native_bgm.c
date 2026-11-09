/* Borrow the game's radio/BGM routing state. Never change master/music/SFX volume. */
#include "native_bgm.h"
#include "controls.h"
#include <string.h>
typedef void *(*Ptr1)(void *);
typedef const char *(*Name)(void *);
typedef void *(*Class)(void *,const char *,const char *);
typedef void *(*Method)(void *,const char *,int);
typedef unsigned (*Count)(void *);
typedef unsigned (*Flags)(void *,unsigned *);
typedef int (*Kind)(void *);
typedef void *(*Param)(void *,void **);
typedef void *(*Invoke)(void *,void *,void **,void **);
typedef void *(*NewString)(void *,const char *);
typedef unsigned (*Pin)(void *,int);
typedef void (*Free)(unsigned);
typedef int (*Bool0)(void);
static Invoke invoke;
static NewString new_string;
static Pin pin;
static Free release;
static Bool0 shutting_down;
static void *domain,*instance,*set_state;
static volatile LONG ready,enabled=1,external_playing,owned;
static ULONGLONG last_apply;
static RadioLog logger;
int bgm_prepare(HMODULE runtime,void *root,void *image,const char *guid,RadioLog log){
    InterlockedExchange(&ready,0);domain=root;logger=log;
    Name image_guid=(Name)GetProcAddress(runtime,"mono_image_get_guid");
    Class klass=(Class)GetProcAddress(runtime,"mono_class_from_name");Method method=(Method)GetProcAddress(runtime,"mono_class_get_method_from_name");
    Ptr1 sig=(Ptr1)GetProcAddress(runtime,"mono_method_signature"),ret=(Ptr1)GetProcAddress(runtime,"mono_signature_get_return_type"),from_type=(Ptr1)GetProcAddress(runtime,"mono_class_from_mono_type");
    Count count=(Count)GetProcAddress(runtime,"mono_signature_get_param_count");Flags flags=(Flags)GetProcAddress(runtime,"mono_method_get_flags");Kind kind=(Kind)GetProcAddress(runtime,"mono_type_get_type");Param parameter=(Param)GetProcAddress(runtime,"mono_signature_get_params");
    invoke=(Invoke)GetProcAddress(runtime,"mono_runtime_invoke");new_string=(NewString)GetProcAddress(runtime,"mono_string_new");
    pin=(Pin)GetProcAddress(runtime,"mono_gchandle_new");release=(Free)GetProcAddress(runtime,"mono_gchandle_free");shutting_down=(Bool0)GetProcAddress(runtime,"mono_runtime_is_shutting_down");
    if(!image||!guid||!image_guid||!klass||!method||!sig||!ret||!from_type||!count||!flags||!kind||!parameter||!invoke||!new_string||!pin||!release||!shutting_down)return 1;
    const char *actual=image_guid(image);if(!actual||_stricmp(actual,guid))return 2;
    void *audio=klass(image,"","AudioSystem");if(!audio)return 3;
    instance=method(audio,"get_Instance",0);set_state=method(audio,"SetState",3);if(!instance||!set_state)return 4;
    void *s=sig(instance);if(!s||!(flags(instance,NULL)&0x10)||count(s)||kind(ret(s))!=18||from_type(ret(s))!=audio)return 5;
    s=sig(set_state);if(!s||(flags(set_state,NULL)&0x10)||count(s)!=3||kind(ret(s))!=1)return 5;
    void *iter=NULL;int types[]={14,14,2};
    for(int i=0;i<3;++i){void *arg=parameter(s,&iter);if(!arg||kind(arg)!=types[i])return 5;}
    InterlockedExchange(&ready,1);
    if(logger)logger("BGM routing ready: audited AudioSystem radio state; no volume overrides");return 0;
}
int bgm_available(void){return (int)InterlockedCompareExchange(&ready,0,0);}
int bgm_active(void){return (int)InterlockedCompareExchange(&owned,0,0);}
void bgm_request(int value,int playing){InterlockedExchange(&enabled,value!=0);InterlockedExchange(&external_playing,playing!=0);}
static int apply(int playing){
    if(!bgm_available()||shutting_down())return 0;
    void *exception=NULL,*audio=invoke(instance,NULL,NULL,&exception);if(exception||!audio)return 0;
    unsigned roots[3]={0};roots[0]=pin(audio,1);if(!roots[0])return 0;
    void *group=new_string(domain,"system_motor_radio");if(!group)goto fail;
    roots[1]=pin(group,1);if(!roots[1])goto fail;
    void *state=new_string(domain,playing?"playing":"none");if(!state)goto fail;
    roots[2]=pin(state,1);if(!roots[2])goto fail;
    unsigned char validate=1;void *args[]={group,state,&validate};invoke(set_state,audio,args,&exception);
    for(int i=2;i>=0;--i)if(roots[i])release(roots[i]);
    if(exception){if(logger)logger("BGM routing call failed; restore will be retried");return 0;}
    return 1;
fail:
    for(int i=2;i>=0;--i)if(roots[i])release(roots[i]);return 0;
}
void bgm_release(int native_playing){
    if(GetCurrentThreadId()!=controls_game_thread()||!bgm_active())return;
    if(apply(native_playing)){InterlockedExchange(&owned,0);if(logger)logger("External BGM override released; native radio routing restored");}
}
void bgm_sync(int native_playing){
    if(GetCurrentThreadId()!=controls_game_thread()||!bgm_available())return;
    int wanted=InterlockedCompareExchange(&enabled,0,0)&&InterlockedCompareExchange(&external_playing,0,0);
    if(!wanted){bgm_release(native_playing);return;}
    ULONGLONG now=GetTickCount64();if(bgm_active()&&now-last_apply<1000)return;
    if(apply(1)){last_apply=now;if(!InterlockedExchange(&owned,1)&&logger)logger("External playback active; game BGM radio routing requested");}
}
