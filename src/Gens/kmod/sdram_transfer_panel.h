#ifndef KMOD_SDRAM_TRANSFER_PANEL_H
#define KMOD_SDRAM_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void sdramtransfer_create(HINSTANCE instance, HWND parent);
void sdramtransfer_show(BOOL visible);
void sdramtransfer_update(void);
void sdramtransfer_destroy(void);
void sdramtransfer_save_window(const char *config);
void sdramtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
