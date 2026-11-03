#pragma once
#include "bridge.h"
#ifdef __cplusplus
extern "C" {
#endif
void controls_start(HMODULE module, const wchar_t *config, RadioLog log);
void controls_capture(void);
void controls_session_end(void);
void controls_shutdown(void);
int controls_post(int command);
DWORD controls_game_thread(void);
#ifdef __cplusplus
}
#endif
