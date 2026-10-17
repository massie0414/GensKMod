#ifndef KMOD_CDGFX_H
#define KMOD_CDGFX_H

#ifdef __cplusplus
extern "C" {
#endif

void cdgfx_create(HINSTANCE hInstance, HWND hWndParent);
void cdgfx_show(BOOL visibility);
void cdgfx_update();
void cdgfx_reset();
void cdgfx_destroy();
void cdgfx_save_window(const char *config_file);
void cdgfx_restore_window(const char *config_file);

#ifdef __cplusplus
};
#endif

#endif //KMOD_CDGFX_H
