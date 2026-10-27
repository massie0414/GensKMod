#include <windows.h>
#include <stdio.h>
#include "../gens.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "../sdram_transfer.h"
#include "common.h"
#include "sdram_transfer_panel.h"
#include "frame_graph.h"
#include "window_geometry.h"

static HWND transfer_window;

static unsigned long GraphScale(void)
{
    unsigned long values[SDRAM_TRANSFER_HISTORY], scale = 4096;
    unsigned int count, i;
    int write;
    if (!_32X_Started && !CD_32X_Active) return scale;
    for (write = 0; write < 2; ++write) {
        count = SDRAMTransfer_GetHistory(write, values);
        for (i = 0; i < count; ++i)
            while (scale < values[i] && scale <= 0x7fffffffUL) scale *= 2;
    }
    return scale;
}

static void SetTextIfChanged(int id, const char *text)
{
    char previous[160];
    GetDlgItemText(transfer_window, id, previous, sizeof(previous));
    if (strcmp(text, previous)) SetDlgItemText(transfer_window, id, text);
}

void sdramtransfer_update(void)
{
    char text[160];
    unsigned long values[SDRAM_TRANSFER_HISTORY];
    unsigned int count;
    int write, active = _32X_Started || CD_32X_Active;
    if (!transfer_window) return;
    count = active ? SDRAMTransfer_GetHistory(SDRAM_TRANSFER_READ, values) : 0;
    for (write = 0; write < 2; ++write) {
        if (count)
            sprintf(text, "Average (up to 1 emulated second): %.1f KiB/s",
                SDRAMTransfer_GetBytesPerSecond(write) / 1024.0);
        else lstrcpy(text, "Average: --");
        SetTextIfChanged(write ? IDC_SDRAM_WRITE_RATE : IDC_SDRAM_READ_RATE, text);
        InvalidateRect(GetDlgItem(transfer_window,
            write ? IDC_SDRAM_WRITE_GRAPH : IDC_SDRAM_READ_GRAPH), NULL, FALSE);
    }
    if (!active) lstrcpy(text, "Emulation inactive");
    else if (!count) lstrcpy(text, "Waiting for a completed frame...");
    else sprintf(text, "32X main RAM / 256 KiB%s", Paused ? " / Paused (history retained)" : "");
    SetTextIfChanged(IDC_SDRAM_TRANSFER_STATUS, text);
}

static INT_PTR CALLBACK TransferProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        transfer_window = hwnd;
        HandleWindow_KMod[DMODE_SDRAM_TRANSFER - 1] = hwnd;
        SetTimer(hwnd, 1, 100, NULL);
        sdramtransfer_update();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) sdramtransfer_update();
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_SDRAM_READ_GRAPH || wParam == IDC_SDRAM_WRITE_GRAPH) {
            unsigned long values[SDRAM_TRANSFER_HISTORY], scale = GraphScale();
            int write = wParam == IDC_SDRAM_WRITE_GRAPH;
            unsigned int count = (_32X_Started || CD_32X_Active) ? SDRAMTransfer_GetHistory(write, values) : 0;
            char latest[80], top[32], middle[32];
            if (count) sprintf(latest, "%.2f KiB / frame (%lu bytes)", values[count - 1] / 1024.0, values[count - 1]);
            else lstrcpy(latest, "--");
            sprintf(top, "%lu", scale / 1024);
            sprintf(middle, "%lu", scale / 2048);
            FrameGraph_Draw((DRAWITEMSTRUCT *)lParam, values, count, scale, latest, top, middle);
            return TRUE;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) != IDCANCEL) return FALSE;
        /* Escape and the close button both hide the persistent panel. */
    case WM_CLOSE:
        CloseWindow_KMod(DMODE_SDRAM_TRANSFER);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        transfer_window = NULL;
        HandleWindow_KMod[DMODE_SDRAM_TRANSFER - 1] = NULL;
        OpenedWindow_KMod[DMODE_SDRAM_TRANSFER - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void sdramtransfer_create(HINSTANCE instance, HWND parent)
{
    transfer_window = CreateDialog(instance, MAKEINTRESOURCE(IDD_SDRAM_TRANSFER), parent, TransferProc);
}

void sdramtransfer_show(BOOL visible)
{
    if (!transfer_window) return;
    if (visible) sdramtransfer_update();
    ShowWindow(transfer_window, visible ? SW_SHOW : SW_HIDE);
}

void sdramtransfer_destroy(void)
{
    if (transfer_window) DestroyWindow(transfer_window);
}

void sdramtransfer_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "SDRAMTransferOpen",
        OpenedWindow_KMod[DMODE_SDRAM_TRANSFER - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(transfer_window, "SDRAMTransferRect", config);
}

void sdramtransfer_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "SDRAMTransferOpen", 0, config) != 0;
    if (!transfer_window) return;
    GetWindowRect(transfer_window, &rect);
    DebugWindow_RestoreGeometry(transfer_window, "SDRAMTransferRect", config);
    SetWindowPos(transfer_window, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_SDRAM_TRANSFER - 1] = visible;
    sdramtransfer_show(visible);
}
