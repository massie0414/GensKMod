/* Build with x86 MSVC and link the Release Gens.res, user32 and comctl32. */
#include <assert.h>
#include "../src/Gens/kmod/sdram_32x.c"
unsigned char _32X_Ram[256 * 1024];
unsigned short _32X_VDP_CRam[256];
int _32X_Started, CD_32X_Active;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode) {
    OpenedWindow_KMod[mode - 1] = 0; sdram32x_show(FALSE);
}
int main(void) {
    unsigned mode, size, page, x, y, offset;
    static unsigned visited[262144];
    char text[2048], dir[MAX_PATH], config[MAX_PATH];
    INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&controls);
    sdram32x_create(GetModuleHandle(NULL), NULL); assert(sdramWindow);
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "inactive"));
    _32X_Started = 1; RefreshSDRAM();
    GetDlgItemText(sdramWindow, IDC_32XSDRAM_SUMMARY, text, sizeof(text));
    assert(strstr(text, "0.00%") && strstr(text, "Zero bytes: 262144"));
    for (mode = 0; mode < 2; ++mode)
        for (size = 0; size < 6; ++size) {
            SendDlgItemMessage(sdramWindow, IDC_32XSDRAM_MODE, CB_SETCURSEL, mode, 0);
            SendMessage(sdramWindow, WM_COMMAND, MAKEWPARAM(IDC_32XSDRAM_MODE, CBN_SELCHANGE), 0);
            SendDlgItemMessage(sdramWindow, IDC_32XSDRAM_SIZE, CB_SETCURSEL, size, 0);
            SendMessage(sdramWindow, WM_COMMAND, MAKEWPARAM(IDC_32XSDRAM_SIZE, CBN_SELCHANGE), 0);
            assert(colorMode == mode && tileSize == (8u << size));
            memset(visited, 0, sizeof(visited));
            for (page = 0; page < sizeof(_32X_Ram) / PageBytes(); ++page) {
                imagePage = page;
                assert(PixelOffset(0, 0) == page * PageBytes());
                assert(PixelOffset(0, 1) == page * PageBytes() + tileSize * (mode + 1));
                if (tileSize < 256) assert(PixelOffset(tileSize, 0) == page * PageBytes() + tileSize * tileSize * (mode + 1));
                for (y = 0; y < 256; ++y) for (x = 0; x < 256; ++x) {
                    offset = PixelOffset(x, y);
                    assert(offset + mode < sizeof(_32X_Ram));
                    assert(!visited[offset]++);
                    if (mode) assert(!visited[offset + 1]++);
                }
            }
            for (offset = 0; offset < sizeof(_32X_Ram); ++offset) assert(visited[offset] == 1);
        }
    colorMode = 0; _32X_Ram[0] = 7; _32X_VDP_CRam[7] = 0x001f;
    assert(PixelColor(0) == 0xff0000);
    _32X_VDP_CRam[7] = 0x83e0; assert(PixelColor(0) == 0x00ff00);
    colorMode = 1; _32X_Ram[0] = 0x7c; _32X_Ram[1] = 0;
    assert(PixelColor(0) == 0x0000ff);
    _32X_Ram[0] = 0xff; _32X_Ram[1] = 0xff; assert(PixelColor(0) == 0xffffff);
    imagePage = 0; RefreshImage();
    assert(!IsWindowEnabled(GetDlgItem(sdramWindow, IDC_32XSDRAM_PREV)));
    SendMessage(sdramWindow, WM_COMMAND, IDC_32XSDRAM_NEXT, 0);
    assert(imagePage == 1 && !IsWindowEnabled(GetDlgItem(sdramWindow, IDC_32XSDRAM_NEXT)));
    SendMessage(sdramWindow, WM_COMMAND, IDC_32XSDRAM_NEXT, 0); assert(imagePage == 1);
    SendMessage(sdramWindow, WM_COMMAND, IDC_32XSDRAM_PREV, 0); assert(imagePage == 0);
    memset(_32X_Ram, 0, sizeof(_32X_Ram));
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
    puts("PASS: both color modes, all six tile sizes, full SDRAM coverage, RGB555, paging, inactive, usage, CD32X, timer, persistence, Escape, cleanup");
    return 0;
}
