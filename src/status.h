#pragma once
#include <windows.h>
#include <stdint.h>
#include <string>

struct RadioStatus {
    std::wstring source, title, artist, state, detail;
    bool view_open=false;
    bool riding=false;
    bool bgm_enabled=true,bgm_available=false,bgm_active=false;
    uint64_t music_id=0;
    std::wstring local_path;
    int64_t position_ms=0,duration_ms=0;
    bool timeline=false,playing=false;
    int rate_permille=1000;
};
DWORD status_hud_pid();
void status_start(HMODULE module, const wchar_t *config);
void status_publish(const RadioStatus &status, bool reveal = false);
void status_stop();
