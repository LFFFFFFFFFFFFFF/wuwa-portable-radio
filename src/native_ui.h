#pragma once
#include "bridge.h"
#ifdef __cplusplus
extern "C" {
#endif
int native_ui_prepare(HMODULE runtime,void *domain,void *image);
void native_ui_refresh(void);
int native_ui_visible(void);
void native_ui_reset(void);
#ifdef __cplusplus
}
#endif
