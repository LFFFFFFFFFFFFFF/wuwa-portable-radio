#pragma once
#include <windows.h>
#include <string>

struct RadioStatus {
    std::wstring source, title, artist, state, detail;
};
void status_start(HMODULE module, const wchar_t *config);
void status_publish(const RadioStatus &status, bool reveal = false);
void status_stop();
