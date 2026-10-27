#ifndef KMOD_CD_TRANSFER_PANEL_H
#define KMOD_CD_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void cdtransfer_create(HINSTANCE instance, HWND parent);
void cdtransfer_show(BOOL visible);
void cdtransfer_update(void);
void cdtransfer_destroy(void);
void cdtransfer_save_window(const char *config);
void cdtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
