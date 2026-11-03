#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
typedef void (*RadioLog)(const char *);
#ifdef __cplusplus
extern "C" {
#endif
int radio_prepare(HMODULE runtime, void *domain, void *image, const char *guid, uint32_t token, RadioLog log);
int radio_enable(void);
int radio_remove(void);
void radio_set_enabled(int enabled);
LONG radio_skipped(void);
/* Commands must run on the observed managed game thread. */
int radio_control(int command);
#ifdef __cplusplus
}
#endif
