#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "../gens.h"
#include "../resource.h"
#include "../mem_sh2.h"
#include "../vdp_io.h"
#include "common.h"
#include "window_geometry.h"
#include "sdram_32x.h"

static HWND sdramWindow;

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
        SendDlgItemMessage(hwnd, IDC_32XSDRAM_USAGE, PBM_SETRANGE32, 0, 1000);
        SetTimer(hwnd, 1, 250, NULL);
        RefreshSDRAM();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) RefreshSDRAM();
        return TRUE;
    case WM_COMMAND:
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
    BOOL visible = GetPrivateProfileInt("DebugWindows", "32XSDRAMOpen", 0, config) != 0;
    DebugWindow_RestoreGeometry(sdramWindow, "32XSDRAMRect", config);
    OpenedWindow_KMod[DMODE_32_SDRAM - 1] = visible && sdramWindow != NULL;
    sdram32x_show(visible);
}
