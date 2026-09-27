#ifndef KMOD_FPS_32X_H
#define KMOD_FPS_32X_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
void fps32x_create(HINSTANCE instance, HWND parent);
void fps32x_show(BOOL visible);
void fps32x_framebuffer_changed(void);
void fps32x_reset(void);
void fps32x_destroy(void);
void fps32x_save_window(const char *config);
void fps32x_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
