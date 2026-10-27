#ifndef KMOD_VRAM_TRANSFER_PANEL_H
#define KMOD_VRAM_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void vramtransfer_create(HINSTANCE instance, HWND parent);
void vramtransfer_show(BOOL visible);
void vramtransfer_update(void);
void vramtransfer_destroy(void);
void vramtransfer_save_window(const char *config);
void vramtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
