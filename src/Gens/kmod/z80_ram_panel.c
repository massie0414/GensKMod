#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "../gens.h"
#include "../Mem_Z80.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "common.h"
#include "window_geometry.h"
#include "z80_ram_panel.h"

#define RAM_TIMER 1
#define RAM_REFRESH_MS 1000

static HWND ramWindow;
static int displayedUsed;

static void RefreshRAM(void)
{
    unsigned i;
    int used = -1;
    char text[96];
    if (!ramWindow || !IsWindowVisible(ramWindow) || IsIconic(ramWindow)) return;
    if (Genesis_Started || SegaCD_Started || _32X_Started) {
        used = 0;
        /* Z80_Reset fills all 8 KB with $00. Zero-valued code/data also
         * counts as free: this measures current contents, not allocation. */
        for (i = 0; i < sizeof(Ram_Z80); ++i)
            if (Ram_Z80[i] != 0) ++used;
    }
    /* Avoid text updates and synchronous progress-bar paints when unchanged. */
    if (used == displayedUsed) return;
    displayedUsed = used;
    if (used >= 0)
        sprintf(text, "%.2f%%  (%u / %u bytes)", used * 100.0 / sizeof(Ram_Z80),
            (unsigned)used, (unsigned)sizeof(Ram_Z80));
    else
        lstrcpy(text, "-- (no active game)");
    SetDlgItemText(ramWindow, IDC_Z80_RAM_SUMMARY, text);
    SendDlgItemMessage(ramWindow, IDC_Z80_RAM_USAGE, PBM_SETPOS, used < 0 ? 0 : used, 0);
}

static INT_PTR CALLBACK RAMProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        ramWindow = hwnd;
        HandleWindow_KMod[DMODE_Z80_RAM - 1] = hwnd;
        displayedUsed = -2;
        SendDlgItemMessage(hwnd, IDC_Z80_RAM_USAGE, PBM_SETRANGE32, 0, sizeof(Ram_Z80));
        return TRUE;
    case WM_SHOWWINDOW:
        if (wParam && !IsIconic(hwnd)) SetTimer(hwnd, RAM_TIMER, RAM_REFRESH_MS, NULL);
        else KillTimer(hwnd, RAM_TIMER);
        return TRUE;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) KillTimer(hwnd, RAM_TIMER);
        else if (IsWindowVisible(hwnd)) {
            SetTimer(hwnd, RAM_TIMER, RAM_REFRESH_MS, NULL);
            RefreshRAM();
        }
        return TRUE;
    case WM_TIMER:
        if (wParam == RAM_TIMER) RefreshRAM();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) != IDCANCEL) return FALSE;
        /* Escape and the close button hide this panel independently of Z80 CPU. */
    case WM_CLOSE:
        CloseWindow_KMod(DMODE_Z80_RAM);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, RAM_TIMER);
        ramWindow = NULL;
        HandleWindow_KMod[DMODE_Z80_RAM - 1] = NULL;
        OpenedWindow_KMod[DMODE_Z80_RAM - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void z80ram_create(HINSTANCE instance, HWND parent)
{
    ramWindow = CreateDialog(instance, MAKEINTRESOURCE(IDD_Z80_RAM), parent, RAMProc);
}

void z80ram_show(BOOL visible)
{
    if (!ramWindow) return;
    ShowWindow(ramWindow, visible ? SW_SHOW : SW_HIDE);
    if (visible) RefreshRAM();
}

void z80ram_reset(void)
{
    RefreshRAM();
}

void z80ram_destroy(void)
{
    if (ramWindow) DestroyWindow(ramWindow);
}

void z80ram_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "Z80RAMOpen",
        OpenedWindow_KMod[DMODE_Z80_RAM - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(ramWindow, "Z80RAMRect", config);
}

void z80ram_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "Z80RAMOpen", 0, config) != 0;
    if (!ramWindow) return;
    GetWindowRect(ramWindow, &rect);
    DebugWindow_RestoreGeometry(ramWindow, "Z80RAMRect", config);
    /* The panel has fixed content; restore its position and retain its size. */
    SetWindowPos(ramWindow, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_Z80_RAM - 1] = visible;
    z80ram_show(visible);
}
