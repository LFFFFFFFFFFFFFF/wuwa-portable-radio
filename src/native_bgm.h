#pragma once
#include "bridge.h"
#ifdef __cplusplus
extern "C" {
#endif
int bgm_prepare(HMODULE runtime,void *domain,void *image,const char *guid,RadioLog log);
void bgm_request(int enabled,int external_playing);
void bgm_sync(int native_playing);
void bgm_release(int native_playing);
int bgm_available(void);
int bgm_active(void);
#ifdef __cplusplus
}
#endif
