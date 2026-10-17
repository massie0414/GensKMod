#ifndef KMOD_CDPRG_H
#define KMOD_CDPRG_H

#ifdef __cplusplus
extern "C" {
#endif

void cdprg_create(HINSTANCE hInstance, HWND hWndParent);
void cdprg_show(BOOL visibility);
void cdprg_update();
void cdprg_reset();
void cdprg_destroy();
void cdprg_save_window(const char *config_file);
void cdprg_restore_window(const char *config_file);

#ifdef __cplusplus
};
#endif

#endif //KMOD_CDPRG_H
