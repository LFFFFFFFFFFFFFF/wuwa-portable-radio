#include "native_ui.h"
#include "controls.h"
typedef void *(*Ptr1)(void *);
typedef void *(*FindClass)(void *,const char *,const char *);
typedef void *(*FindMember)(void *,const char *,int);
typedef void *(*Field)(void *,const char *);
typedef unsigned (*Count)(void *);
typedef int (*Kind)(void *);
typedef unsigned (*Flags)(void *,unsigned *);
typedef void *(*Param)(void *,void **);
typedef void *(*Invoke)(void *,void *,void **,void **);
typedef void *(*FieldObject)(void *,void *,void *);
typedef unsigned (*Pin)(void *,int);
typedef void (*Free)(unsigned);
static Invoke invoke;
static Ptr1 unbox,object_class;
static FieldObject field_object;
static Pin pin;
static Free release;
static void *domain,*instance,*show,*view_field,*view_class;
static volatile LONG visible;
static int ready;
int native_ui_prepare(HMODULE runtime,void *root,void *image){
    ready=0;domain=root;
    FindClass klass=(FindClass)GetProcAddress(runtime,"mono_class_from_name");
    FindMember find=(FindMember)GetProcAddress(runtime,"mono_class_get_method_from_name");
    Field field=(Field)GetProcAddress(runtime,"mono_class_get_field_from_name");
    Ptr1 signature=(Ptr1)GetProcAddress(runtime,"mono_method_signature"),ret=(Ptr1)GetProcAddress(runtime,"mono_signature_get_return_type");
    Ptr1 from_type=(Ptr1)GetProcAddress(runtime,"mono_class_from_mono_type"),field_type=(Ptr1)GetProcAddress(runtime,"mono_field_get_type");
    Count count=(Count)GetProcAddress(runtime,"mono_signature_get_param_count");Kind kind=(Kind)GetProcAddress(runtime,"mono_type_get_type");
    Flags flags=(Flags)GetProcAddress(runtime,"mono_method_get_flags");Param parameter=(Param)GetProcAddress(runtime,"mono_signature_get_params");
    Count field_flags=(Count)GetProcAddress(runtime,"mono_field_get_flags");
    invoke=(Invoke)GetProcAddress(runtime,"mono_runtime_invoke");unbox=(Ptr1)GetProcAddress(runtime,"mono_object_unbox");
    object_class=(Ptr1)GetProcAddress(runtime,"mono_object_get_class");field_object=(FieldObject)GetProcAddress(runtime,"mono_field_get_value_object");
    pin=(Pin)GetProcAddress(runtime,"mono_gchandle_new");release=(Free)GetProcAddress(runtime,"mono_gchandle_free");
    if(!klass||!find||!field||!signature||!ret||!from_type||!field_type||!count||!kind||!flags||!parameter||!field_flags||!invoke||!unbox||!object_class||!field_object||!pin||!release)return 1;
    void *manager=klass(image,"CSharpScript.Game.Ui","UiManager");view_class=klass(image,"CSharpScript.Game.Ui","EUiViewName");
    if(!manager||!view_class)return 2;
    instance=find(manager,"get_Instance",0);show=find(manager,"IsViewShow",1);view_field=field(view_class,"MotorcycleMusicPlayerView");
    if(!instance||!show||!view_field)return 3;
    void *sig=signature(instance);
    if(!(flags(instance,NULL)&0x10)||!sig||count(sig)||kind(ret(sig))!=18||from_type(ret(sig))!=manager)return 4;
    sig=signature(show);void *iter=NULL;
    if((flags(show,NULL)&0x10)||!sig||count(sig)!=1||kind(ret(sig))!=2)return 4;
    void *arg=parameter(sig,&iter);
    if(!arg||kind(arg)!=17||from_type(arg)!=view_class||!(field_flags(view_field)&0x10)||from_type(field_type(view_field))!=view_class)return 4;
    ready=1;return 0;
}
void native_ui_refresh(void){
    if(!ready||GetCurrentThreadId()!=controls_game_thread())return;
    void *exception=NULL;void *manager=invoke(instance,NULL,NULL,&exception);
    if(exception||!manager){InterlockedExchange(&visible,0);return;}
    void *name=field_object(domain,view_field,NULL);
    if(!name||object_class(name)!=view_class){InterlockedExchange(&visible,0);return;}
    unsigned root=pin(name,1);if(!root)return;
    void *args[]={unbox(name)};void *result=invoke(show,manager,args,&exception);
    int value=!exception&&result&&*(unsigned char *)unbox(result);
    release(root);InterlockedExchange(&visible,value);
}
int native_ui_visible(void){return (int)InterlockedCompareExchange(&visible,0,0);}
void native_ui_reset(void){InterlockedExchange(&visible,0);}
