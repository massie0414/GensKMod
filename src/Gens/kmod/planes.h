#ifndef KMOD_PLANES_H
#define KMOD_PLANES_H


#ifdef __cplusplus
extern "C" {
#endif


void planes_create(HINSTANCE hInstance, HWND hWndParent);
void planes_show(int plane, BOOL visibility);
void planes_update();
void planes_reset();
void planes_destroy();
void planes_save_visibility(const char *config_file);
void planes_restore_visibility(const char *config_file);


#ifdef __cplusplus
};
#endif

#endif //KMOD_PLANES_H