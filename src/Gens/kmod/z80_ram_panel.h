#ifndef KMOD_Z80_RAM_PANEL_H
#define KMOD_Z80_RAM_PANEL_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

void z80ram_create(HINSTANCE instance, HWND parent);
void z80ram_show(BOOL visible);
void z80ram_reset(void);
void z80ram_destroy(void);
void z80ram_save_window(const char *config);
void z80ram_restore_window(const char *config);

#ifdef __cplusplus
}
#endif
#endif
