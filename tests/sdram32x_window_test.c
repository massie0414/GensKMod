/* Build with x86 MSVC and link the Release Gens.res, user32 and comctl32. */
#include <assert.h>
#include "../src/Gens/kmod/sdram_32x.c"
unsigned char _32X_Ram[256 * 1024];
int _32X_Started, CD_32X_Active;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode) {
    OpenedWindow_KMod[mode - 1] = 0; sdram32x_show(FALSE);
}
int main(void) {
    char text[2048], dir[MAX_PATH], config[MAX_PATH];
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);
    sdram32x_create(GetModuleHandle(NULL), NULL); assert(sdramWindow);
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "inactive"));
    _32X_Started = 1; RefreshSDRAM();
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "0.00%") && strstr(text, "Zero bytes: 262144"));
    memset(_32X_Ram, 1, sizeof(_32X_Ram) / 2); RefreshSDRAM();
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "50.00%") && strstr(text, "131072 / 262144"));
    assert(SendDlgItemMessage(sdramWindow, IDC_32XSDRAM_USAGE, PBM_GETPOS, 0, 0) == 500);
    memset(_32X_Ram, 255, sizeof(_32X_Ram));
    _32X_Started = 0; CD_32X_Active = 1; RefreshSDRAM();
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "100.00%"));
    memset(_32X_Ram, 0, sizeof(_32X_Ram)); _32X_Ram[262143] = 1; RefreshSDRAM();
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_BLOCKS, text, sizeof(text));
    assert(strstr(text, "0x0603C000 - 0x0603FFFF") && strstr(text, "(    1 / 16384)"));
    GetTempPath(sizeof(dir), dir); GetTempFileName(dir, "sdr", 0, config);
    OpenedWindow_KMod[DMODE_32_SDRAM - 1] = 1; sdram32x_show(TRUE);
    sdram32x_save_window(config); sdram32x_destroy();
    sdram32x_create(GetModuleHandle(NULL), NULL); sdram32x_restore_window(config);
    assert(IsWindowVisible(sdramWindow));
    CD_32X_Active = 0; SendMessage(sdramWindow, WM_TIMER, 1, 0);
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "inactive"));
    SendMessage(sdramWindow, WM_COMMAND, IDCANCEL, 0);
    assert(!IsWindowVisible(sdramWindow) && !OpenedWindow_KMod[DMODE_32_SDRAM - 1]);
    sdram32x_destroy(); assert(!HandleWindow_KMod[DMODE_32_SDRAM - 1]);
    DeleteFile(config);
    puts("PASS: inactive, zero/half/full, block boundaries, CD32X, timer, persistence, Escape, cleanup");
    return 0;
}
