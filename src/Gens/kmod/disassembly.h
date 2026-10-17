#ifndef KMOD_DISASSEMBLY_H
#define KMOD_DISASSEMBLY_H
#ifdef __cplusplus
extern "C" {
#endif
void disassembly_create(HINSTANCE instance, HWND parent);
void disassembly_show(BOOL visible);
void disassembly_reset(void);
void disassembly_clear(void);
void disassembly_save_window(const char *config);
void disassembly_restore_window(const char *config);
#ifdef __cplusplus
}
#endif
#endif
