#ifndef KMOD_VRAM32X_TRANSFER_PANEL_H
#define KMOD_VRAM32X_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void vram32xtransfer_create(HINSTANCE instance, HWND parent);
void vram32xtransfer_show(BOOL visible);
void vram32xtransfer_update(void);
void vram32xtransfer_destroy(void);
void vram32xtransfer_save_window(const char *config);
void vram32xtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
