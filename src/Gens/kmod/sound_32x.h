#ifndef KMOD_SOUND_32X_H
#define KMOD_SOUND_32X_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
void sound32x_create(HINSTANCE instance, HWND parent);
void sound32x_show(BOOL visible);
void sound32x_sample(int length);
void sound32x_reset(void);
void sound32x_destroy(void);
void sound32x_save_window(const char *config);
void sound32x_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
