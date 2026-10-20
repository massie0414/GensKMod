#ifndef KMOD_VDP_REG_H
#define KMOD_VDP_REG_H

extern UCHAR pal_KMod;

#ifdef __cplusplus
extern "C" {
#endif

void vdpreg_create(HINSTANCE hInstance, HWND hWndParent);
void vdpreg_show(BOOL visibility);
void vdpreg_update();
void vdpreg_reset();
void vdpreg_destroy();
void vdpreg_save_window(const char *config_file);
void vdpreg_restore_window(const char *config_file);

#ifdef __cplusplus
};
#endif

#endif //KMOD_VDP_REG_H