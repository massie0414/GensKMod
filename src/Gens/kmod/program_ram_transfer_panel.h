#ifndef KMOD_PROGRAM_RAM_TRANSFER_PANEL_H
#define KMOD_PROGRAM_RAM_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void programramtransfer_create(HINSTANCE instance, HWND parent);
void programramtransfer_show(BOOL visible);
void programramtransfer_update(void);
void programramtransfer_destroy(void);
void programramtransfer_save_window(const char *config);
void programramtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
