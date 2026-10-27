#ifndef KMOD_SH2_DMA_TRANSFER_PANEL_H
#define KMOD_SH2_DMA_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void sh2dmatransfer_create(HINSTANCE instance, HWND parent);
void sh2dmatransfer_show(int kind, BOOL visible);
void sh2dmatransfer_update(void);
void sh2dmatransfer_destroy(void);
void sh2dmatransfer_save_window(const char *config);
void sh2dmatransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
