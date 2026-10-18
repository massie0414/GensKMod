#ifndef KMOD_YM2612_H
#define KMOD_YM2612_H

#ifdef __cplusplus
extern "C" {
#endif

void ym2612_create(HINSTANCE hInstance, HWND hWndParent);
void ym2612_show(BOOL visibility);
int ym2612_level_active(void);
void ym2612_level_sample(int channel, int left, int right);
void ym2612_save_window(const char *config);
void ym2612_restore_window(const char *config);
void ym2612_update();
void ym2612_reset();
void ym2612_destroy();

#ifdef __cplusplus
};
#endif

#endif //KMOD_YM2612_H