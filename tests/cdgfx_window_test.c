#include <assert.h>
#include "../src/Gens/kmod/cdgfx.c"

unsigned char Ram_Word_2M[256 * 1024];
UCHAR pal_KMod;
char Rom_Name[512];
COLORREF vdpdebug_getColor(unsigned int palette, unsigned int color) { return RGB(color, color, color); }
void Put_Info(const char *message, int duration) { (void)message; (void)duration; }
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    cdgfx_show(FALSE);
}

static void CheckTileSelection(void)
{
    HWND image = GetDlgItem(hCD_GFX, IDC_CD_GFX);
    RECT rect, preview, client;
    char address[32];
    DRAWITEMSTRUCT draw = {0};
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 64, 128);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    GetClientRect(image, &rect);
    SendMessage(image, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rect.right - 1, rect.bottom - 1));
    assert(selectedTile == 4095);
    GetDlgItemText(hCD_GFX, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x03FFC0"));
    SendMessage(image, WM_KEYDOWN, VK_RIGHT, 0);
    assert(selectedTile == 4095);
    SendMessage(image, WM_KEYDOWN, VK_LEFT, 0);
    assert(selectedTile == 4094);
    GetDlgItemText(hCD_GFX, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x03FF80"));
    SendMessage(image, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(0, 0));
    assert(selectedTile == 0);
    SendMessage(image, WM_KEYDOWN, VK_LEFT, 0);
    assert(selectedTile == 0);
    SendMessage(image, WM_KEYDOWN, VK_RIGHT, 0);
    assert(selectedTile == 1);
    GetDlgItemText(hCD_GFX, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x000040"));
    CD_GFXSetSelectedTile(hCD_GFX, 127);
    SendMessage(image, WM_KEYDOWN, VK_RIGHT, 0);
    assert(selectedTile == 128);
    SendMessage(hCD_GFX, WM_KEYDOWN, VK_LEFT, 0);
    assert(selectedTile == 127);
    assert(SendMessage(image, WM_GETDLGCODE, VK_LEFT, 0) & DLGC_WANTARROWS);
    SendMessage(image, WM_KEYDOWN, VK_UP, 0);
    assert(selectedTile == 127);
    SendMessage(image, WM_KEYDOWN, VK_DOWN, 0);
    assert(selectedTile == 255);
    GetDlgItemText(hCD_GFX, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x003FC0"));
    SendMessage(hCD_GFX, WM_KEYDOWN, VK_UP, 0);
    assert(selectedTile == 127);
    CD_GFXSetSelectedTile(hCD_GFX, 4095);
    SendMessage(image, WM_KEYDOWN, VK_DOWN, 0);
    assert(selectedTile == 4095);
    SendMessage(image, WM_KEYDOWN, VK_UP, 0);
    assert(selectedTile == 3967);
    CD_GFXSetSelectedTile(hCD_GFX, 0);
    Ram_Word_2M[1] = 0xAB;
    Ram_Word_2M[0] = 0xCD;
    assert(CD_GFXPixel(0, 0, 0) == 10 && CD_GFXPixel(0, 1, 0) == 11);
    assert(CD_GFXPixel(0, 2, 0) == 12 && CD_GFXPixel(0, 3, 0) == 13);
    draw.hDC = dc;
    SetRect(&draw.rcItem, 0, 0, 64, 128);
    CD_GFXDrawPreview(&draw);
    assert(GetPixel(dc, 0, 0) == RGB(10, 10, 10));
    assert(GetPixel(dc, 7, 7) == RGB(10, 10, 10));
    assert(GetPixel(dc, 8, 0) == RGB(11, 11, 11));
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    GetWindowRect(GetDlgItem(hCD_GFX, CD_GFX_PREVIEW_ID), &preview);
    MapWindowPoints(NULL, hCD_GFX, (LPPOINT)&preview, 2);
    GetClientRect(hCD_GFX, &client);
    assert(preview.bottom <= client.bottom && preview.right <= client.right);
    /* Hit testing must also work when the overview is scaled to fit a monitor. */
    SetWindowPos(image, NULL, 0, 0, 512, 256, SWP_NOMOVE | SWP_NOZORDER);
    SendMessage(image, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(4, 8));
    assert(selectedTile == 129);
    GetDlgItemText(hCD_GFX, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x002040"));
    CD_GFXSelectTile(hCD_GFX, -1, 0);
    assert(selectedTile == 129);
    cdgfx_reset();
    assert(selectedTile == 129);
}

int main(void)
{
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_LISTVIEW_CLASSES};
    char folder[MAX_PATH], config[MAX_PATH];
    RECT saved, restored;
    MONITORINFO monitor = {sizeof(monitor)};
    InitCommonControlsEx(&ic);
    GetTempPath(sizeof(folder), folder);
    assert(GetTempFileName(folder, "cdg", 0, config));
    cdgfx_create(GetModuleHandle(NULL), NULL);
    assert(hCD_GFX && HandleWindow_KMod[DMODE_CD_GFX - 1] == hCD_GFX);
    cdgfx_restore_window(config);
    assert(!IsWindowVisible(hCD_GFX));
    CheckTileSelection();
    cdgfx_reset();
    GetMonitorInfo(MonitorFromWindow(hCD_GFX, MONITOR_DEFAULTTONEAREST), &monitor);
    SetWindowPos(hCD_GFX, NULL, monitor.rcWork.left + 40, monitor.rcWork.top + 40,
        0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_CD_GFX - 1] = TRUE;
    cdgfx_show(TRUE);
    GetWindowRect(hCD_GFX, &saved);
    cdgfx_save_window(config);
    cdgfx_destroy();
    assert(!hCD_GFX && !HandleWindow_KMod[DMODE_CD_GFX - 1]);
    cdgfx_create(GetModuleHandle(NULL), NULL);
    cdgfx_restore_window(config);
    assert(IsWindowVisible(hCD_GFX) && OpenedWindow_KMod[DMODE_CD_GFX - 1]);
    GetWindowRect(hCD_GFX, &restored);
    assert(EqualRect(&saved, &restored));
    assert(!(GetWindowLong(hCD_GFX, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX)));
    SendMessage(hCD_GFX, WM_COMMAND, IDCANCEL, 0);
    assert(!IsWindowVisible(hCD_GFX) && !OpenedWindow_KMod[DMODE_CD_GFX - 1]);
    cdgfx_save_window(config);
    cdgfx_destroy();
    cdgfx_create(GetModuleHandle(NULL), NULL);
    cdgfx_restore_window(config);
    assert(!IsWindowVisible(hCD_GFX) && !OpenedWindow_KMod[DMODE_CD_GFX - 1]);
    cdgfx_destroy();
    DeleteFile(config);
    puts("PASS: tile hit testing, addresses, 8x preview pixels, reset, position and visibility restoration");
    return 0;
}

