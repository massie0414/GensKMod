#ifndef KMOD_PWM_FIFO_TRANSFER_PANEL_H
#define KMOD_PWM_FIFO_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void pwmfifotransfer_create(HINSTANCE instance, HWND parent);
void pwmfifotransfer_show(BOOL visible);
void pwmfifotransfer_update(void);
void pwmfifotransfer_destroy(void);
void pwmfifotransfer_save_window(const char *config);
void pwmfifotransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
