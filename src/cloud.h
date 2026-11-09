#pragma once
#include <stdint.h>
#include <string>
int cloud_command(int command);
int cloud_open(uint64_t id);
struct CloudStatus {
    std::wstring title, artist;
    int playback=0;
    bool available=false;
    int64_t position_ms=0,duration_ms=0;
    bool timeline=false;
    int rate_permille=1000;
};
void cloud_watch_start();
void cloud_watch_stop();
CloudStatus cloud_status();
