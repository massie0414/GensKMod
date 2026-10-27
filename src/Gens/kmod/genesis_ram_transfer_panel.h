#ifndef KMOD_GENESIS_RAM_TRANSFER_PANEL_H
#define KMOD_GENESIS_RAM_TRANSFER_PANEL_H
#ifdef __cplusplus
extern "C" {
#endif
void genesisramtransfer_create(HINSTANCE instance, HWND parent);
void genesisramtransfer_show(int kind, BOOL visible);
void genesisramtransfer_update(void);
void genesisramtransfer_destroy(void);
void genesisramtransfer_save_window(const char *config);
void genesisramtransfer_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
