/* Native radio candidate: late ASI entry, no profiler or Mono startup options. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "bridge.h"
#include "controls.h"
#include "native_bgm.h"

typedef void *(*Ptr0)(void);
typedef void *(*Ptr1)(void *);
typedef void (*Void1)(void *);
typedef const char *(*String1)(void *);
typedef int (*Bool0)(void);
typedef void (*Each)(void (*)(void *, void *), void *);
static HMODULE self_module, runtime;
static volatile LONG once, status;
static wchar_t log_path[32768];

#ifdef RADIO_LAB
#include "fixture_identity.h"
#define EXPECTED_CORE_GUID EXPECTED_GUID
#define INITIAL_DELAY 10
#define POLL_DELAY 20
#else
#define EXPECTED_GUID "630e3143-f5eb-42e7-940c-5f1af45f3fb6"
#define EXPECTED_CORE_GUID "f2c9e329-0f5e-4676-9aa1-d7c93bbdb1d1"
#define EXPECTED_TOKEN 0x060116c2u
#define INITIAL_DELAY 15000
#define POLL_DELAY 2000
#endif

static void log_line(const char *message) {
    FILE *file = NULL;
    if (_wfopen_s(&file, log_path, L"ab") || !file) return;
    SYSTEMTIME t; GetSystemTime(&t);
    fprintf(file, "%04u-%02u-%02uT%02u:%02u:%02uZ pid=%lu %s\n",
        t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,GetCurrentProcessId(),message);
    fclose(file);
}
static int init_paths(void) {
    wchar_t exe[32768];
    DWORD n = GetModuleFileNameW(NULL,exe,32768);
    if (!n || n >= 32768) return 0;
    wchar_t *p = wcsrchr(exe,L'\\'); if (!p) return 0;
#ifndef RADIO_LAB
    if (_wcsicmp(p+1,L"Client-Win64-Shipping.exe")) return 0;
#endif
    *p = 0;
    if (wcscpy_s(log_path,32768,exe)) return 0;
#ifndef RADIO_LAB
    const wchar_t suffix[] = L"\\Client\\Binaries\\Win64";
    size_t size = wcslen(log_path), tail = wcslen(suffix);
    if (size < tail || _wcsicmp(log_path+size-tail,suffix)) return 0;
    for (int i=0;i<2;++i) { p=wcsrchr(log_path,L'\\');if(!p)return 0;*p=0; }
    return !wcscat_s(log_path,32768,L"\\Saved\\Logs\\PortableRadioNative.log");
#else
    return !wcscat_s(log_path,32768,L"\\PortableRadioNative.lab.log");
#endif
}

struct Search { void *image,*audio; unsigned matching,other,audio_matches,audio_other; };
static void find_game(void *assembly, void *context) {
    struct Search *s = context;
    Ptr1 get_name = (Ptr1)GetProcAddress(runtime,"mono_assembly_get_name");
    Ptr1 get_image = (Ptr1)GetProcAddress(runtime,"mono_assembly_get_image");
    String1 name = (String1)GetProcAddress(runtime,"mono_assembly_name_get_name");
    String1 guid = (String1)GetProcAddress(runtime,"mono_image_get_guid");
    void *aname = get_name(assembly);
    const char *simple = aname ? name(aname) : NULL;
    if (!simple || (strcmp(simple,"Aki.Game")&&strcmp(simple,"Aki.Core"))) return;
    void *image = get_image(assembly);
    const char *id = image ? guid(image) : NULL;
    if(!strcmp(simple,"Aki.Core")){
        if(id&&!_stricmp(id,EXPECTED_CORE_GUID)){s->audio=image;++s->audio_matches;}else ++s->audio_other;
        return;
    }
    if (id && !_stricmp(id,EXPECTED_GUID)) { s->image=image; ++s->matching; }
    else ++s->other;
#ifdef RADIO_LAB
    s->audio=s->image;s->audio_matches=s->matching;
#endif
    /* No type loading or game invocation within assembly enumeration. */
}

static DWORD WINAPI install_worker(void *unused) {
    (void)unused;
    Sleep(INITIAL_DELAY);
    for (int attempt=0;attempt<90;++attempt) {
        runtime=GetModuleHandleW(L"coreclr.dll");
        if (!runtime) { Sleep(POLL_DELAY);continue; }
#ifndef RADIO_LAB
        wchar_t runtime_path[32768], expected[32768];
        DWORD length=GetModuleFileNameW(runtime,runtime_path,32768);
        DWORD exe_length=GetModuleFileNameW(NULL,expected,32768);
        if (!length || length>=32768 || !exe_length || exe_length>=32768) break;
        wchar_t *slash=wcsrchr(expected,L'\\');if(!slash)break;*slash=0;
        if (wcscat_s(expected,32768,L"\\coreclr.dll") || _wcsicmp(expected,runtime_path)) {
            log_line("Runtime path mismatch; disabled without hooking");break;
        }
#endif
        Ptr0 root=(Ptr0)GetProcAddress(runtime,"mono_get_root_domain");
        Ptr1 attach=(Ptr1)GetProcAddress(runtime,"mono_thread_attach");
        Void1 detach=(Void1)GetProcAddress(runtime,"mono_thread_detach");
        Each each=(Each)GetProcAddress(runtime,"mono_assembly_foreach");
        Bool0 shutting_down=(Bool0)GetProcAddress(runtime,"mono_runtime_is_shutting_down");
        if (!root||!attach||!detach||!each||!shutting_down||
            !GetProcAddress(runtime,"mono_assembly_get_image")||!GetProcAddress(runtime,"mono_assembly_get_name")||
            !GetProcAddress(runtime,"mono_assembly_name_get_name")||!GetProcAddress(runtime,"mono_image_get_guid")) break;
        if(shutting_down())break;
        void *domain=root();
        if(!domain) { Sleep(POLL_DELAY);continue; }
        void *thread=attach(domain);if(!thread)break;
        struct Search search={0};each(find_game,&search);
        int prepared=-1;
        if(search.matching==1 && search.other==0)
            prepared=radio_prepare(runtime,domain,search.image,EXPECTED_GUID,EXPECTED_TOKEN,log_line);
        if(!prepared){
            int bgm_result=search.audio_matches==1&&!search.audio_other?bgm_prepare(runtime,domain,search.audio,EXPECTED_CORE_GUID,log_line):1;
            if(bgm_result){char line[96];sprintf_s(line,sizeof(line),"BGM interface unavailable: stage=%d; no routing changed",bgm_result);log_line(line);}
        }
        detach(thread);
        if(search.other || search.matching>1) {
            log_line("Assembly identity/count mismatch; disabled without hooking");break;
        }
        if(!search.matching) { Sleep(POLL_DELAY);continue; }
        if(prepared) {
            char line[96];sprintf_s(line,sizeof(line),"Compatibility check failed: stage=%d; no hook installed",prepared);log_line(line);break;
        }
        int result=radio_enable();
        if(result) {
            char line[96];sprintf_s(line,sizeof(line),"Hook installation failed: stage=%d",result);log_line(line);break;
        }
        InterlockedExchange(&status,2);
        wchar_t config[32768];wcscpy_s(config,32768,log_path);
        wchar_t *last=wcsrchr(config,L'\\');if(last)*last=0;
#ifndef RADIO_LAB
        last=wcsrchr(config,L'\\');if(last)*last=0;
        wcscat_s(config,32768,L"\\Config\\PortableRadio.ini");
#else
        wcscat_s(config,32768,L"\\PortableRadio.ini");
#endif
        controls_start(self_module,config,log_line);
        return 0;
    }
    InterlockedExchange(&status,-1);
    log_line("Native radio inactive; original dismount behavior retained");
    return 1;
}

__declspec(dllexport) void InitializeASI(void) {
    if(InterlockedCompareExchange(&once,1,0))return;
    if(!init_paths())return;
    log_line("Portable radio v2.8 initialized; synchronized song subtitles and automatic scene attachment");
    /* Keep code alive until process exit while a managed method can call its detour. */
    wchar_t module_path[32768];HMODULE pinned;
    DWORD n=GetModuleFileNameW(self_module,module_path,32768);
    if(!n||n>=32768||!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN,module_path,&pinned)) {
        log_line("Module lifetime could not be pinned; disabled");return;
    }
    HANDLE worker=CreateThread(NULL,0,install_worker,NULL,0,NULL);
    if(!worker) { log_line("Initialization worker failed; disabled");return; }
    CloseHandle(worker);InterlockedCompareExchange(&status,1,0);
}
__declspec(dllexport) LONG PortableRadioStatus(void) { return InterlockedCompareExchange(&status,0,0); }
#ifdef RADIO_LAB
__declspec(dllexport) int PortableRadioLabRemove(void) { controls_shutdown();return radio_remove(); }
__declspec(dllexport) DWORD PortableRadioLabThread(void) { return controls_game_thread(); }
__declspec(dllexport) int PortableRadioLabPost(int command) { return controls_post(command); }
#endif
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    (void)reserved;if(reason==DLL_PROCESS_ATTACH)self_module=module;return TRUE;
}
