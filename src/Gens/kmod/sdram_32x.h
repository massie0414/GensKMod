#ifndef KMOD_SDRAM_32X_H
#define KMOD_SDRAM_32X_H
#ifdef __cplusplus
extern "C" {
#endif
void sdram32x_create(HINSTANCE instance, HWND parent);
void sdram32x_show(BOOL visible);
void sdram32x_reset(void);
void sdram32x_destroy(void);
void sdram32x_save_window(const char *config);
void sdram32x_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
