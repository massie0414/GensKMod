#ifndef KMOD_MEMORY_32X_H
#define KMOD_MEMORY_32X_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
void memory32x_create(HINSTANCE instance, HWND parent);
void memory32x_show(BOOL visible);
void memory32x_save_window(const char *config);
void memory32x_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
