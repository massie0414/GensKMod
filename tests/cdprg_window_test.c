#include <assert.h>
#include "../src/Gens/kmod/cdprg.c"

unsigned char Ram_Prg[512 * 1024];
UCHAR pal_KMod;
char Rom_Name[512];
COLORREF vdpdebug_getColor(unsigned int palette, unsigned int color) { return RGB(color, color, color); }
void Put_Info(const char *message, int duration) { (void)message; (void)duration; }
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    cdprg_show(FALSE);
}

static void CheckTileSelection(void)
{
    HWND image = GetDlgItem(hCD_PRG, IDC_CD_GFX);
    RECT rect, preview, client;
    char address[32];
    DRAWITEMSTRUCT draw = {0};
    HDC screen = GetDC(NULL), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, 64, 128);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    GetClientRect(image, &rect);
    SendMessage(image, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(rect.right - 1, rect.bottom - 1));
    assert(programSelectedTile == 8191);
    GetDlgItemText(hCD_PRG, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x07FFC0"));
    SendMessage(image, WM_KEYDOWN, VK_RIGHT, 0);
    assert(programSelectedTile == 8191);
    SendMessage(image, WM_KEYDOWN, VK_LEFT, 0);
    assert(programSelectedTile == 8190);
    GetDlgItemText(hCD_PRG, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x07FF80"));
    SendMessage(image, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(0, 0));
    assert(programSelectedTile == 0);
    SendMessage(image, WM_KEYDOWN, VK_LEFT, 0);
    assert(programSelectedTile == 0);
    SendMessage(image, WM_KEYDOWN, VK_RIGHT, 0);
    assert(programSelectedTile == 1);
    GetDlgItemText(hCD_PRG, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x000040"));
    CD_PRGSetSelectedTile(hCD_PRG, 127);
    SendMessage(image, WM_KEYDOWN, VK_RIGHT, 0);
    assert(programSelectedTile == 128);
    SendMessage(hCD_PRG, WM_KEYDOWN, VK_LEFT, 0);
    assert(programSelectedTile == 127);
    assert(SendMessage(image, WM_GETDLGCODE, VK_LEFT, 0) & DLGC_WANTARROWS);
    SendMessage(image, WM_KEYDOWN, VK_UP, 0);
    assert(programSelectedTile == 127);
    SendMessage(image, WM_KEYDOWN, VK_DOWN, 0);
    assert(programSelectedTile == 255);
    GetDlgItemText(hCD_PRG, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x003FC0"));
    SendMessage(hCD_PRG, WM_KEYDOWN, VK_UP, 0);
    assert(programSelectedTile == 127);
    CD_PRGSetSelectedTile(hCD_PRG, 8191);
    SendMessage(image, WM_KEYDOWN, VK_DOWN, 0);
    assert(programSelectedTile == 8191);
    SendMessage(image, WM_KEYDOWN, VK_UP, 0);
    assert(programSelectedTile == 8063);
    /* Exercise bytes in the upper half and the final byte of Program RAM. */
    Ram_Prg[0x40001] = 0xEF;
    assert(CD_PRGPixel(4096, 0, 0) == 14);
    assert(CD_PRGPixel(4096, 1, 0) == 15);
    Ram_Prg[0x7FFFF] = 0xAB;
    assert(CD_PRGPixel(8191, 4, 15) == 10);
    assert(CD_PRGPixel(8191, 5, 15) == 11);
    CD_PRGSetSelectedTile(hCD_PRG, 0);
    Ram_Prg[1] = 0xAB;
    Ram_Prg[0] = 0xCD;
    assert(CD_PRGPixel(0, 0, 0) == 10 && CD_PRGPixel(0, 1, 0) == 11);
    assert(CD_PRGPixel(0, 2, 0) == 12 && CD_PRGPixel(0, 3, 0) == 13);
    draw.hDC = dc;
    SetRect(&draw.rcItem, 0, 0, 64, 128);
    CD_PRGDrawPreview(&draw);
    assert(GetPixel(dc, 0, 0) == RGB(10, 10, 10));
    assert(GetPixel(dc, 7, 7) == RGB(10, 10, 10));
    assert(GetPixel(dc, 8, 0) == RGB(11, 11, 11));
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(NULL, screen);
    GetWindowRect(GetDlgItem(hCD_PRG, CD_PRG_PREVIEW_ID), &preview);
    MapWindowPoints(NULL, hCD_PRG, (LPPOINT)&preview, 2);
    GetClientRect(hCD_PRG, &client);
    assert(preview.bottom <= client.bottom && preview.right <= client.right);
    /* Hit testing must also work when the overview is scaled to fit a monitor. */
    SetWindowPos(image, NULL, 0, 0, 512, 512, SWP_NOMOVE | SWP_NOZORDER);
    SendMessage(image, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(4, 8));
    assert(programSelectedTile == 129);
    GetDlgItemText(hCD_PRG, IDC_CD_GFX_ADR, address, sizeof(address));
    assert(!strcmp(address, "0x002040"));
    CD_PRGSelectTile(hCD_PRG, -1, 0);
    assert(programSelectedTile == 129);
    cdprg_reset();
    assert(programSelectedTile == 129);
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
    cdprg_create(GetModuleHandle(NULL), NULL);
    assert(hCD_PRG && HandleWindow_KMod[DMODE_CD_PRG - 1] == hCD_PRG);
    cdprg_restore_window(config);
    assert(!IsWindowVisible(hCD_PRG));
    CheckTileSelection();
    cdprg_reset();
    GetMonitorInfo(MonitorFromWindow(hCD_PRG, MONITOR_DEFAULTTONEAREST), &monitor);
    SetWindowPos(hCD_PRG, NULL, monitor.rcWork.left + 40, monitor.rcWork.top + 40,
        0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_CD_PRG - 1] = TRUE;
    cdprg_show(TRUE);
    GetWindowRect(hCD_PRG, &saved);
    cdprg_save_window(config);
    cdprg_destroy();
    assert(!hCD_PRG && !HandleWindow_KMod[DMODE_CD_PRG - 1]);
    cdprg_create(GetModuleHandle(NULL), NULL);
    cdprg_restore_window(config);
    assert(IsWindowVisible(hCD_PRG) && OpenedWindow_KMod[DMODE_CD_PRG - 1]);
    GetWindowRect(hCD_PRG, &restored);
    assert(EqualRect(&saved, &restored));
    assert(!(GetWindowLong(hCD_PRG, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX)));
    SendMessage(hCD_PRG, WM_COMMAND, IDCANCEL, 0);
    assert(!IsWindowVisible(hCD_PRG) && !OpenedWindow_KMod[DMODE_CD_PRG - 1]);
    cdprg_save_window(config);
    cdprg_destroy();
    cdprg_create(GetModuleHandle(NULL), NULL);
    cdprg_restore_window(config);
    assert(!IsWindowVisible(hCD_PRG) && !OpenedWindow_KMod[DMODE_CD_PRG - 1]);
    cdprg_destroy();
    DeleteFile(config);
    puts("PASS: tile hit testing, addresses, 8x preview pixels, reset, position and visibility restoration");
    return 0;
}

