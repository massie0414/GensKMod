#ifndef KMOD_VDP_SPRITES_H
#define KMOD_VDP_SPRITES_H

#ifdef __cplusplus
extern "C" {
#endif

void sprites_create(HINSTANCE hInstance, HWND hWndParent);
void sprites_show(BOOL visibility);
void sprites_update();
void sprites_reset();
void sprites_destroy();
void sprites_save_window(const char *config_file);
void sprites_restore_window(const char *config_file);

#ifdef __cplusplus
};
#endif

#endif //KMOD_VDP_SPRITES_H