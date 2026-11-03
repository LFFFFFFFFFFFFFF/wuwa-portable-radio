#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <algorithm>
#include <string>
#include <stdio.h>
#include "cloud.h"
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
