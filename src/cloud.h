#pragma once
#include <stdint.h>
#include <string>
int cloud_command(int command);
int cloud_open(uint64_t id);
struct CloudStatus {
    std::wstring title, artist;
    int playback=0;
    bool available=false;
};
void cloud_watch_start();
void cloud_watch_stop();
CloudStatus cloud_status();
