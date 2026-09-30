#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "../gens.h"
#include "../resource.h"
#include "../mem_sh2.h"
#include "../vdp_io.h"
#include "../vdp_32X.h"
#include "common.h"
#include "window_geometry.h"
#include "sdram_32x.h"

static HWND sdramWindow;
static unsigned colorMode, tileSize = 8, imagePage;
static DWORD imagePixels[256 * 256];

static unsigned PageBytes(void) { return 256 * 256 * (colorMode ? 2 : 1); }

/* Consecutive tiles, then row-major pixels within each tile. SDRAM is
   stored in SH2 byte order, unlike the host-word framebuffer storage. */
static unsigned PixelOffset(unsigned x, unsigned y)
{
    unsigned tile = (y / tileSize) * (256 / tileSize) + x / tileSize;
    return imagePage * PageBytes() +
        (tile * tileSize * tileSize + (y % tileSize) * tileSize + x % tileSize) * (colorMode ? 2 : 1);
}

static DWORD PixelColor(unsigned offset)
{
    unsigned color = colorMode ? ((_32X_Ram[offset] << 8) | _32X_Ram[offset + 1]) :
        _32X_VDP_CRam[_32X_Ram[offset]];
    unsigned r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
    /* A 32-bit BI_RGB DIB uses B,G,R bytes; bit 15 is priority, not alpha. */
    return (((r << 3) | (r >> 2)) << 16) |
        (((g << 3) | (g >> 2)) << 8) | (b << 3) | (b >> 2);
}

static void RefreshImage(void)
{
    char text[160];
    unsigned bytes = PageBytes();
    if (imagePage >= sizeof(_32X_Ram) / bytes) imagePage = sizeof(_32X_Ram) / bytes - 1;
    sprintf(text, "0x%08X - 0x%08X  (%u/%u)\r\n%u bytes/tile%s",
        0x06000000 + imagePage * bytes, 0x06000000 + (imagePage + 1) * bytes - 1,
        imagePage + 1, (unsigned)sizeof(_32X_Ram) / bytes,
        tileSize * tileSize * (colorMode ? 2 : 1), colorMode ? " - RGB555" : " - 32X palette");
    SetDlgItemText(sdramWindow, IDC_32XSDRAM_ADDRESS, text);
    EnableWindow(GetDlgItem(sdramWindow, IDC_32XSDRAM_PREV), imagePage > 0);
    EnableWindow(GetDlgItem(sdramWindow, IDC_32XSDRAM_NEXT), (imagePage + 1) * bytes < sizeof(_32X_Ram));
    InvalidateRect(GetDlgItem(sdramWindow, IDC_32XSDRAM_IMAGE), NULL, FALSE);
}

static void DrawImage(const DRAWITEMSTRUCT *draw)
{
    BITMAPINFO info;
    unsigned x, y;
    int width = draw->rcItem.right - draw->rcItem.left;
    int height = draw->rcItem.bottom - draw->rcItem.top;
    int scale = min(width, height) / 256;
    RECT rect = draw->rcItem;
    FillRect(draw->hDC, &rect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    if (!_32X_Started && !CD_32X_Active)
    {
        SetTextColor(draw->hDC, RGB(255,255,255));
        SetBkMode(draw->hDC, TRANSPARENT);
        DrawText(draw->hDC, "No active 32X session.", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    for (y = 0; y < 256; ++y)
        for (x = 0; x < 256; ++x) imagePixels[y * 256 + x] = PixelColor(PixelOffset(x, y));
    memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 256;
    info.bmiHeader.biHeight = -256;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    if (scale < 1) scale = 1;
    SetStretchBltMode(draw->hDC, COLORONCOLOR);
    StretchDIBits(draw->hDC, rect.left + (width - 256 * scale) / 2,
        rect.top + (height - 256 * scale) / 2, 256 * scale, 256 * scale,
        0, 0, 256, 256, imagePixels, &info, DIB_RGB_COLORS, SRCCOPY);
}

static void SetTextIfChanged(int id, const char *text)
{
    char previous[2048];
    GetDlgItemText(sdramWindow, id, previous, sizeof(previous));
    if (strcmp(previous, text)) SetDlgItemText(sdramWindow, id, text);
}

static void RefreshSDRAM(void)
{
    unsigned counts[16] = {0}, total = 0, i;
    char summary[512], blocks[2048], *out = blocks;
    if (!sdramWindow) return;
    RefreshImage();
    if (!_32X_Started && !CD_32X_Active)
    {
        SetTextIfChanged(IDC_32XSDRAM_SUMMARY,
            "32X inactive - memory usage: --\r\nCapacity: 256 KiB (262144 bytes)\r\nSH2: 0x06000000 - 0x0603FFFF");
        SetTextIfChanged(IDC_32XSDRAM_BLOCKS, "No active 32X session.");
        SendDlgItemMessage(sdramWindow, IDC_32XSDRAM_USAGE, PBM_SETPOS, 0, 0);
        return;
    }
    /* Read on the UI/emulation thread. No writes or allocation tracking.
       Byte order does not affect the number of non-zero bytes in a block. */
    for (i = 0; i < sizeof(_32X_Ram); ++i)
        if (_32X_Ram[i]) { ++counts[i / 16384]; ++total; }
    sprintf(summary,
        "Capacity: 256 KiB (262144 bytes)\r\nSH2: 0x06000000 - 0x0603FFFF\r\nMemory usage (non-zero): %.2f%%  (%u / 262144 bytes)\r\nZero bytes: %u",
        total * 100.0 / sizeof(_32X_Ram), total, (unsigned)sizeof(_32X_Ram) - total);
    for (i = 0; i < 16; ++i)
        out += sprintf(out, "0x%08X - 0x%08X    %5.1f%%   (%5u / 16384)\r\n",
            0x06000000 + i * 16384, 0x06000000 + (i + 1) * 16384 - 1,
            counts[i] * 100.0 / 16384, counts[i]);
    SetTextIfChanged(IDC_32XSDRAM_SUMMARY, summary);
    SetTextIfChanged(IDC_32XSDRAM_BLOCKS, blocks);
    SendDlgItemMessage(sdramWindow, IDC_32XSDRAM_USAGE, PBM_SETPOS,
        total * 1000 / sizeof(_32X_Ram), 0);
}

static INT_PTR CALLBACK SDRAMProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        sdramWindow = hwnd;
        HandleWindow_KMod[DMODE_32_SDRAM - 1] = hwnd;
        {
            unsigned i;
            char label[32];
            colorMode = imagePage = 0;
            tileSize = 8;
            SendDlgItemMessage(hwnd, IDC_32XSDRAM_MODE, CB_ADDSTRING, 0, (LPARAM)"256 colors (32X palette)");
            SendDlgItemMessage(hwnd, IDC_32XSDRAM_MODE, CB_ADDSTRING, 0, (LPARAM)"Direct color (RGB555)");
            for (i = 8; i <= 256; i *= 2)
            {
                sprintf(label, "%u x %u", i, i);
                SendDlgItemMessage(hwnd, IDC_32XSDRAM_SIZE, CB_ADDSTRING, 0, (LPARAM)label);
            }
            SendDlgItemMessage(hwnd, IDC_32XSDRAM_MODE, CB_SETCURSEL, 0, 0);
            SendDlgItemMessage(hwnd, IDC_32XSDRAM_SIZE, CB_SETCURSEL, 0, 0);
        }
        SendDlgItemMessage(hwnd, IDC_32XSDRAM_USAGE, PBM_SETRANGE32, 0, 1000);
        SetTimer(hwnd, 1, 250, NULL);
        RefreshSDRAM();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) RefreshSDRAM();
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            if (LOWORD(wParam) == IDC_32XSDRAM_MODE)
            {
                unsigned offset = imagePage * PageBytes();
                colorMode = (unsigned)SendDlgItemMessage(hwnd, IDC_32XSDRAM_MODE, CB_GETCURSEL, 0, 0);
                imagePage = offset / PageBytes();
            }
            else if (LOWORD(wParam) == IDC_32XSDRAM_SIZE)
                tileSize = 8u << SendDlgItemMessage(hwnd, IDC_32XSDRAM_SIZE, CB_GETCURSEL, 0, 0);
            RefreshImage();
            return TRUE;
        }
        if (LOWORD(wParam) == IDC_32XSDRAM_PREV || LOWORD(wParam) == IDC_32XSDRAM_NEXT)
        {
            if (LOWORD(wParam) == IDC_32XSDRAM_PREV) { if (imagePage) --imagePage; }
            else if ((imagePage + 1) * PageBytes() < sizeof(_32X_Ram)) ++imagePage;
            RefreshImage();
            return TRUE;
        }
        if (LOWORD(wParam) != IDCANCEL) return FALSE;
        /* Escape uses the same persistent hide behavior as the close button. */
    case WM_CLOSE:
        CloseWindow_KMod(DMODE_32_SDRAM);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        sdramWindow = NULL;
        HandleWindow_KMod[DMODE_32_SDRAM - 1] = NULL;
        OpenedWindow_KMod[DMODE_32_SDRAM - 1] = FALSE;
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_32XSDRAM_IMAGE) { DrawImage((const DRAWITEMSTRUCT *)lParam); return TRUE; }
        return FALSE;
    }
    return FALSE;
}

void sdram32x_create(HINSTANCE instance, HWND parent)
{
    sdramWindow = CreateDialog(instance, MAKEINTRESOURCE(IDD_DEBUG32X_SDRAM), parent, SDRAMProc);
}
void sdram32x_show(BOOL visible)
{
    if (visible) RefreshSDRAM();
    ShowWindow(sdramWindow, visible ? SW_SHOW : SW_HIDE);
}
void sdram32x_reset(void) { RefreshSDRAM(); }
void sdram32x_destroy(void) { if (sdramWindow) DestroyWindow(sdramWindow); }
void sdram32x_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "32XSDRAMOpen",
        OpenedWindow_KMod[DMODE_32_SDRAM - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(sdramWindow, "32XSDRAMRect", config);
}
void sdram32x_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "32XSDRAMOpen", 0, config) != 0;
    GetWindowRect(sdramWindow, &rect);
    DebugWindow_RestoreGeometry(sdramWindow, "32XSDRAMRect", config);
    /* This fixed-layout dialog grew; do not reuse the old usage-only size. */
    SetWindowPos(sdramWindow, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_32_SDRAM - 1] = visible && sdramWindow != NULL;
    sdram32x_show(visible);
}
