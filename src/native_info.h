#pragma once
#include "bridge.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NativeRadioInfo {
    int supported, observed, track_id, active, playing;
    wchar_t title[512];
} NativeRadioInfo;
int native_info_prepare(HMODULE runtime, void *image, RadioLog log);
/* Refresh only on the observed managed game thread; snapshots are plain copies. */
void native_info_refresh(int force);
NativeRadioInfo native_info_snapshot(void);
void native_info_reset(void);
#ifdef __cplusplus
}
#endif
