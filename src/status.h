#pragma once
#include <windows.h>
#include <string>

struct RadioStatus {
    std::wstring source, title, artist, state, detail;
    bool view_open=false;
    bool riding=false;
    bool bgm_enabled=true,bgm_available=false,bgm_active=false;
};
DWORD status_hud_pid();
void status_start(HMODULE module, const wchar_t *config);
void status_publish(const RadioStatus &status, bool reveal = false);
void status_stop();
