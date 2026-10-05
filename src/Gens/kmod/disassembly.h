#ifndef KMOD_DISASSEMBLY_H
#define KMOD_DISASSEMBLY_H
#ifdef __cplusplus
extern "C" {
#endif
void disassembly_create(HINSTANCE instance, HWND parent);
void disassembly_show(BOOL visible);
void disassembly_reset(void);
void disassembly_clear(void);
#ifdef __cplusplus
}
#endif
#endif
