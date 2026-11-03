/* Full ASI initialization test with synthetic game assembly in independent process. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
typedef void *(*Ptr1)(void *);
typedef void (*Void1)(void *);
typedef void (*SetPath)(const char *);
typedef void *(*Init)(const char *,const char *);
typedef void *(*Open)(void *,const char *);
typedef void *(*Class)(void *,const char *,const char *);
typedef void *(*Method)(void *,const char *,int);
typedef void *(*Field)(void *,const char *);
typedef void (*Value)(void *,void *,void *);
typedef void *(*Invoke)(void *,void *,void **,void **);
typedef void (*InitAsi)(void);
typedef LONG (*Status)(void);
typedef int (*RemoveHook)(void);
#define CHECK(x) do{if(!(x)){printf("FAIL line=%d\n",__LINE__);return 10;}}while(0)
int wmain(int argc,wchar_t **argv) {
    if(argc!=5)return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    HMODULE rt=LoadLibraryExW(argv[1],NULL,LOAD_WITH_ALTERED_SEARCH_PATH);CHECK(rt);
    char path[32768];CHECK(WideCharToMultiByte(CP_UTF8,0,argv[2],-1,path,32768,NULL,NULL));
    ((SetPath)GetProcAddress(rt,"mono_set_assemblies_path"))(path);
    void *domain=((Init)GetProcAddress(rt,"mono_jit_init_version"))("FullAsiLoaderLab","v4.0.30319");CHECK(domain);
    CHECK(WideCharToMultiByte(CP_UTF8,0,argv[3],-1,path,32768,NULL,NULL));
    void *assembly=((Open)GetProcAddress(rt,"mono_domain_assembly_open"))(domain,path);CHECK(assembly);
    void *image=((Ptr1)GetProcAddress(rt,"mono_assembly_get_image"))(assembly);
    Class klass=(Class)GetProcAddress(rt,"mono_class_from_name");
    Method method=(Method)GetProcAddress(rt,"mono_class_get_method_from_name");
    Field field=(Field)GetProcAddress(rt,"mono_class_get_field_from_name");
    Value set=(Value)GetProcAddress(rt,"mono_field_set_value"),read=(Value)GetProcAddress(rt,"mono_field_get_value");
    Invoke invoke=(Invoke)GetProcAddress(rt,"mono_runtime_invoke");
    void *controller=klass(image,"","MotorcycleMusicPlayerController");CHECK(controller);
    typedef void *(*New)(void *,void *);
    void *object=((New)GetProcAddress(rt,"mono_object_new"))(domain,controller);CHECK(object);
    ((Void1)GetProcAddress(rt,"mono_runtime_object_init"))(object);
    typedef uint32_t (*Root)(void *,int);uint32_t root=((Root)GetProcAddress(rt,"mono_gchandle_new"))(object,1);
    void *leave=method(controller,"OnLeaveVehicle",2),*exception=NULL;
    unsigned char change=0;void *args[]={NULL,&change};
    invoke(leave,object,args,&exception);CHECK(!exception);
    int32_t pauses=0;read(object,field(controller,"Pauses"),&pauses);CHECK(pauses==1);
    HMODULE plugin=LoadLibraryW(argv[4]);CHECK(plugin);
    InitAsi initialize=(InitAsi)GetProcAddress(plugin,"InitializeASI");
    Status status=(Status)GetProcAddress(plugin,"PortableRadioStatus");
    RemoveHook remove=(RemoveHook)GetProcAddress(plugin,"PortableRadioLabRemove");CHECK(initialize&&status&&remove);
    CHECK(status()==0);
    for(int i=0;i<160;++i)initialize();
    for(int i=0;i<1000&&status()==1;++i)Sleep(10);
    printf("full_ASI_status=%ld\n",status());CHECK(status()==2);
    int32_t handle=123,zero=0;unsigned char yes=1;
    set(object,field(controller,"CurHandleId"),&handle);set(object,field(controller,"IsPlaying"),&yes);
    set(object,field(controller,"IsInVehicle"),&yes);set(object,field(controller,"Pauses"),&zero);
    invoke(leave,object,args,&exception);CHECK(!exception);
    read(object,field(controller,"Pauses"),&pauses);CHECK(pauses==0);
    unsigned char playing=0;read(object,field(controller,"IsPlaying"),&playing);CHECK(playing==1);
    CHECK(remove()==0);
    invoke(leave,object,args,&exception);CHECK(!exception);
    read(object,field(controller,"Pauses"),&pauses);CHECK(pauses==1);
    typedef void (*FreeRoot)(uint32_t);((FreeRoot)GetProcAddress(rt,"mono_gchandle_free"))(root);
    puts("PASS: full ASI init -> worker attach -> identity/signature/JIT checks -> detach -> hook -> stream retained -> removal restores original");
    return 0;
}
