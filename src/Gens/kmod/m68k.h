#ifndef KMOD_M68K_H
#define KMOD_M68K_H


#ifdef __cplusplus
extern "C" {
#endif


void m68kdebug_create(HINSTANCE hInstance, HWND hWndParent);
void m68kdebug_show(BOOL visibility);
void m68kdebug_update();
void m68kdebug_reset();
void m68kdebug_destroy();
void m68kdebug_save_window(const char *config_file);
void m68kdebug_restore_window(const char *config_file);


void m68kdebug_dump();//TODO remove
void m68kdebug_jumpRAM(DWORD adr);

#ifdef __cplusplus
};
#endif

#endif //KMOD_M68K_H