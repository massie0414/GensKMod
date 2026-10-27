#ifndef KMOD_MAIN_WORD_RAM_TRANSFER_PANEL_H
#define KMOD_MAIN_WORD_RAM_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void mainwordramtransfer_create(HINSTANCE instance, HWND parent);
void mainwordramtransfer_show(BOOL visible);
void mainwordramtransfer_update(void);
void mainwordramtransfer_destroy(void);
void mainwordramtransfer_save_window(const char *config);
void mainwordramtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
