#ifndef KMOD_32X_SSH2_H
#define KMOD_32X_SSH2_H

#ifdef __cplusplus
extern "C" {
#endif

void sSH2_create(HINSTANCE hInstance, HWND hWndParent);
void sSH2_show(BOOL visibility);
void sSH2_update();
void sSH2_reset();
void sSH2_destroy();
void sSH2_save_window(const char *config_file);
void sSH2_restore_window(const char *config_file);

#ifdef __cplusplus
};
#endif

#endif //KMOD_32X_SSH2_H