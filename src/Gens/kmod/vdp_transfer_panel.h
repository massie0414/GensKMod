#ifndef KMOD_VDP_TRANSFER_PANEL_H
#define KMOD_VDP_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void vdptransfer_create(HINSTANCE instance, HWND parent);
void vdptransfer_show(int kind, BOOL visible);
void vdptransfer_update(void);
void vdptransfer_destroy(void);
void vdptransfer_save_window(const char *config);
void vdptransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
