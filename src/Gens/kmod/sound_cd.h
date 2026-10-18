#ifndef KMOD_SOUND_CD_H
#define KMOD_SOUND_CD_H
#include <windows.h>
#ifdef __cplusplus
extern "C" {
#endif
void soundcd_create(HINSTANCE instance, HWND parent);
void soundcd_show(int source, BOOL visible);
void soundcd_reset(void);
void soundcd_save_window(const char *config);
void soundcd_restore_window(const char *config);
int soundcd_active(int source);
void soundcd_sample(int source, int left, int right);
void soundcd_pcm_sample(int channel, int left, int right);
#ifdef __cplusplus
}
#endif
#endif
