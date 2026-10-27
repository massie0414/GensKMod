#ifndef KMOD_WORD_RAM_TRANSFER_PANEL_H
#define KMOD_WORD_RAM_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void wordramtransfer_create(HINSTANCE instance, HWND parent);
void wordramtransfer_show(BOOL visible);
void wordramtransfer_update(void);
void wordramtransfer_destroy(void);
void wordramtransfer_save_window(const char *config);
void wordramtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
