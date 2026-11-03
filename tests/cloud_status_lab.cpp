#include "cloud.h"
#include <windows.h>
#include <stdio.h>
int main(){
    cloud_watch_start();CloudStatus actual;
    for(int i=0;i<60;++i){actual=cloud_status();if(actual.available)break;Sleep(100);}
    cloud_watch_stop();
    if(!actual.available){puts("No NetEase SMTC session; no playback commands were sent.");return 2;}
    // Report lengths/state only, keeping listening history out of diagnostics.
    printf("PASS: NetEase metadata available; title_chars=%zu artist_chars=%zu playback=%d; no playback commands sent\n",actual.title.size(),actual.artist.size(),actual.playback);
    return actual.title.empty()?3:0;
}
