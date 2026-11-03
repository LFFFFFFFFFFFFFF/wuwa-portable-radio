#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include "status.h"

static std::wstring path, previous;
static HANDLE hud;
static unsigned long sequence;
static std::wstring clean(std::wstring value) {
    if(value.size()>512)value.resize(512);
    for(auto &c:value)if(c<L' '||c==0x7f)c=L' ';
    return value;
}
void status_start(HMODULE module,const wchar_t *config) {
    if(!GetPrivateProfileIntW(L"HUD",L"Enabled",1,config))return;
    std::wstring directory=config;
    auto slash=directory.find_last_of(L'\\');
    if(slash==std::wstring::npos)return;
    directory.resize(slash);
    wchar_t file[96];swprintf_s(file,L"\\PortableRadioStatus-%lu.ini",GetCurrentProcessId());
    path=directory+file;
    wchar_t exe[32768];DWORD size=GetModuleFileNameW(module,exe,32768);
    if(!size||size>=32768){path.clear();return;}
    std::wstring helper=exe;slash=helper.find_last_of(L'\\');
    if(slash==std::wstring::npos){path.clear();return;}
    helper.resize(slash);helper+=L"\\RadioHud.exe";
    std::wstring command=L"\""+helper+L"\" --pid "+std::to_wstring(GetCurrentProcessId())+
        L" --status \""+path+L"\" --config \""+config+L"\"";
    STARTUPINFOW startup={};startup.cb=sizeof(startup);
    startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process={};
    if(CreateProcessW(helper.c_str(),command.data(),NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&startup,&process)) {
        CloseHandle(process.hThread);hud=process.hProcess;
    } else path.clear();
}
void status_publish(const RadioStatus &s,bool reveal) {
    if(path.empty())return;
    std::wstring body=L"Source="+clean(s.source)+L"\r\nTitle="+clean(s.title)+L"\r\nArtist="+clean(s.artist)+
        L"\r\nState="+clean(s.state)+L"\r\nDetail="+clean(s.detail)+L"\r\n";
    if(body==previous&&!reveal)return;
    std::wstring text=L"[Radio]\r\nPid="+std::to_wstring(GetCurrentProcessId())+L"\r\nSequence="+
        std::to_wstring(sequence+1)+L"\r\n"+body;
    std::wstring temp=path+L".new";
    FILE *file=nullptr;if(_wfopen_s(&file,temp.c_str(),L"wb")||!file)return;
    unsigned short bom=0xfeff;
    bool ok=fwrite(&bom,sizeof(bom),1,file)==1&&fwrite(text.data(),sizeof(wchar_t),text.size(),file)==text.size();
    if(fclose(file))ok=false;
    if(ok&&MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        previous=body;++sequence;
    }else DeleteFileW(temp.c_str());
}
void status_stop() {
    if(!path.empty())DeleteFileW(path.c_str());
    if(hud){CloseHandle(hud);hud=nullptr;}
    path.clear();previous.clear();
}
