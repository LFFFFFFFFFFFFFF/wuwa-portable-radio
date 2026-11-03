/* Optional read-only native metadata bridge. No additional game detours. */
#include "native_info.h"
#include "controls.h"
#include <string.h>
#include <stdio.h>

typedef void *(*Ptr1)(void *);
typedef const char *(*Name1)(void *);
typedef int (*Int1)(void *);
typedef uint32_t (*Uint1)(void *);
typedef uint32_t (*Flags)(void *,uint32_t *);
typedef void *(*FindClass)(void *,const char *,const char *);
typedef void *(*FindMethod)(void *,const char *,int);
typedef void *(*FindField)(void *,const char *);
typedef void *(*Param)(void *,void **);
typedef void *(*Invoke)(void *,void *,void **,void **);
typedef void (*Read)(void *,void *,void *);
typedef uint32_t (*Root)(void *,int);
typedef void (*Free)(uint32_t);
typedef const wchar_t *(*Chars)(void *);
static Ptr1 signature,return_type,class_from_type,object_class,unbox,class_image;
static Name1 class_name,class_namespace;
static Int1 kind,string_length;
static Uint1 count;
static Flags flags;
static Param parameter;
static Invoke invoke;
static Read read_field;
static Root pin;
static Free release;
static Chars string_chars;
static void *controller_get,*model_get,*config_get,*get_music,*get_title,*localize,*has_value,*get_value;
static void *model_id,*handle,*is_playing,*music_class,*nullable_class;
static SRWLOCK lock=SRWLOCK_INIT;
static NativeRadioInfo snapshot;
static int supported;
static ULONGLONG last_lookup;
static RadioLog logger;

static int named(void *klass,const char *space,const char *name) {
    return klass&&!strcmp(class_namespace(klass),space)&&!strcmp(class_name(klass),name);
}
static int method(void *value,int is_static,int result,unsigned parameters,const int *types) {
    if(!value||!!(flags(value,NULL)&0x10)!=is_static)return 0;
    void *sig=signature(value);
    if(!sig||count(sig)!=parameters||kind(return_type(sig))!=result)return 0;
    void *iter=NULL;
    for(unsigned i=0;i<parameters;++i){void *arg=parameter(sig,&iter);if(!arg||kind(arg)!=types[i])return 0;}
    return 1;
}
int native_info_prepare(HMODULE runtime,void *image,RadioLog log) {
    logger=log;supported=0;
#define API(type,var,name) var=(type)GetProcAddress(runtime,name);if(!var)return 1
    API(Ptr1,signature,"mono_method_signature");API(Ptr1,return_type,"mono_signature_get_return_type");
    API(Ptr1,class_from_type,"mono_class_from_mono_type");API(Ptr1,object_class,"mono_object_get_class");
    API(Ptr1,unbox,"mono_object_unbox");API(Ptr1,class_image,"mono_class_get_image");
    API(Name1,class_name,"mono_class_get_name");API(Name1,class_namespace,"mono_class_get_namespace");
    API(Int1,kind,"mono_type_get_type");API(Int1,string_length,"mono_string_length");
    API(Uint1,count,"mono_signature_get_param_count");API(Flags,flags,"mono_method_get_flags");
    API(Param,parameter,"mono_signature_get_params");API(Invoke,invoke,"mono_runtime_invoke");
    API(Read,read_field,"mono_field_get_value");API(Root,pin,"mono_gchandle_new");
    API(Free,release,"mono_gchandle_free");API(Chars,string_chars,"mono_string_chars");
#undef API
    FindClass klass=(FindClass)GetProcAddress(runtime,"mono_class_from_name");
    FindMethod find=(FindMethod)GetProcAddress(runtime,"mono_class_get_method_from_name");
    FindField field=(FindField)GetProcAddress(runtime,"mono_class_get_field_from_name");
    Ptr1 field_type=(Ptr1)GetProcAddress(runtime,"mono_field_get_type");
    Uint1 field_flags=(Uint1)GetProcAddress(runtime,"mono_field_get_flags");
    if(!klass||!find||!field||!field_type||!field_flags)return 1;
    void *controller=klass(image,"","MotorcycleMusicPlayerController");
    void *model=klass(image,"","MotorcycleMusicPlayerModel");
    void *config=klass(image,"","PhonographConfig");
    if(!controller||!model||!config)return 2;
    controller_get=find(controller,"get_Instance",0);model_get=find(model,"get_Instance",0);config_get=find(config,"get_Instance",0);
    if(!method(controller_get,1,18,0,NULL)||!method(model_get,1,18,0,NULL)||!method(config_get,1,18,0,NULL))return 3;
    if(class_from_type(return_type(signature(controller_get)))!=controller||
       class_from_type(return_type(signature(model_get)))!=model||class_from_type(return_type(signature(config_get)))!=config)return 3;
    model_id=field(model,"CurPlayMusicId");handle=field(controller,"CurHandleId");is_playing=field(controller,"IsPlaying");
    if(!model_id||!handle||!is_playing)return 4;
    if((field_flags(model_id)|field_flags(handle)|field_flags(is_playing))&0x10)return 4;
    if(kind(field_type(model_id))!=8||kind(field_type(handle))!=8||kind(field_type(is_playing))!=2)return 4;
    int integer[]={8};get_music=find(config,"GetMusicById",1);
    if(!method(get_music,0,21,1,integer))return 5;
    nullable_class=class_from_type(return_type(signature(get_music)));
    if(!named(nullable_class,"System","Nullable`1"))return 5;
    has_value=find(nullable_class,"get_HasValue",0);get_value=find(nullable_class,"get_Value",0);
    if(!method(has_value,0,2,0,NULL)||!method(get_value,0,17,0,NULL))return 6;
    music_class=class_from_type(return_type(signature(get_value)));
    if(!named(music_class,"Aki.Config","PhonographMusic"))return 6;
    get_title=find(music_class,"get_Title",0);
    void *text=klass(class_image(music_class),"Aki.Config","ConfigMultiTextLang");
    localize=text?find(text,"GetLocalTextNew",2):NULL;
    int strings[]={14,14};
    if(!method(get_title,0,14,0,NULL)||!method(localize,1,14,2,strings))return 7;
    supported=1;native_info_reset();
    if(logger)logger("Native title reader ready: current music ID -> configuration title -> current-language text");
    return 0;
}
void native_info_reset(void) {
    AcquireSRWLockExclusive(&lock);memset(&snapshot,0,sizeof(snapshot));snapshot.supported=supported;ReleaseSRWLockExclusive(&lock);
    last_lookup=0;
}
NativeRadioInfo native_info_snapshot(void) {
    NativeRadioInfo result;AcquireSRWLockShared(&lock);result=snapshot;ReleaseSRWLockShared(&lock);return result;
}
static void store(NativeRadioInfo *value) {AcquireSRWLockExclusive(&lock);snapshot=*value;ReleaseSRWLockExclusive(&lock);}
static void *call(void *m,void *self,void **args) {
    void *exception=NULL;void *value=invoke(m,self,args,&exception);return exception?NULL:value;
}
static int copy_string(void *value,wchar_t *destination) {
    if(!value||!named(object_class(value),"System","String"))return 0;
    int length=string_length(value);if(length<=0)return 0;
    const wchar_t *source=string_chars(value);if(!source)return 0;
    if(length>511)length=511;
    if(source[length-1]>=0xd800&&source[length-1]<=0xdbff)--length;
    memcpy(destination,source,(size_t)length*sizeof(wchar_t));destination[length]=0;return length>0;
}
void native_info_refresh(int force) {
    if(!supported||GetCurrentThreadId()!=controls_game_thread())return;
    NativeRadioInfo next=native_info_snapshot();next.observed=1;
    int32_t id=0,stream=-1;unsigned char playing=0;
    void *model=call(model_get,NULL,NULL);
    if(model)read_field(model,model_id,&id);
    void *controller=call(controller_get,NULL,NULL);
    if(controller){read_field(controller,handle,&stream);read_field(controller,is_playing,&playing);}
    int changed=id!=next.track_id;
    next.track_id=id;next.active=controller&&model&&stream!=-1;next.playing=next.active&&playing;
    if(changed||!next.active)next.title[0]=0;
    if(!next.active||id<=0){store(&next);return;}
    ULONGLONG now=GetTickCount64();
    if(!force&&!changed&&now-last_lookup<(next.title[0]?10000u:2000u)){store(&next);return;}
    last_lookup=now;next.title[0]=0;
    uint32_t roots[4]={0};
    void *config=call(config_get,NULL,NULL);
    if(!config){store(&next);return;}
    void *args[]={&id};void *boxed=call(get_music,config,args);
    if(!boxed){store(&next);return;}
    roots[0]=pin(boxed,1);
    if(!roots[0]){store(&next);return;}
    void *boxed_class=object_class(boxed),*music=boxed;
    if(boxed_class==nullable_class) {
        void *has=call(has_value,unbox(boxed),NULL);
        if(!has||!*(unsigned char *)unbox(has))goto cleanup;
        music=call(get_value,unbox(boxed),NULL);
        if(!music)goto cleanup;
        roots[1]=pin(music,1);if(!roots[1])goto cleanup;
    }else if(boxed_class!=music_class)goto cleanup;
    if(object_class(music)!=music_class)goto cleanup;
    void *key=call(get_title,unbox(music),NULL);
    if(!key||!named(object_class(key),"System","String"))goto cleanup;
    roots[2]=pin(key,1);if(!roots[2])goto cleanup;
    /* Match the game's existing GetLocalTextNew(key, null) call sites. */
    void *text_args[]={key,NULL};void *text=call(localize,NULL,text_args);
    if(text){roots[3]=pin(text,1);if(roots[3])copy_string(text,next.title);}
cleanup:
    for(int i=3;i>=0;--i)if(roots[i])release(roots[i]);
    store(&next);
    if(changed&&logger){char line[128];sprintf_s(line,sizeof(line),"Native track metadata: id=%ld title=%s playing=%d",(long)id,next.title[0]?"resolved":"unavailable",next.playing);logger(line);}
}
