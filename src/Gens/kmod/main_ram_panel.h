#ifndef KMOD_MAIN_RAM_USAGE_PANEL_H
#define KMOD_MAIN_RAM_USAGE_PANEL_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void mainram_create(HINSTANCE instance, HWND parent);
void mainram_show(BOOL visible);
void mainram_reset(void);
void mainram_destroy(void);
void mainram_save_window(const char *config);
void mainram_restore_window(const char *config);

#ifdef __cplusplus
}
#endif
#endif
