#include <windows.h>
#include <stdio.h>
#include "../gens.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "../vram32x_transfer.h"
#include "common.h"
#include "vram32x_transfer_panel.h"
#include "frame_graph.h"
#include "window_geometry.h"

static HWND transfer_window;

void vram32xtransfer_update(void)
{
    char text[160], previous[160];
    unsigned long values[VRAM32X_TRANSFER_HISTORY];
    unsigned int count;
    if (!transfer_window) return;
    count = (_32X_Started || CD_32X_Active) ? VRAM32XTransfer_GetHistory(values) : 0;
    if (!_32X_Started && !CD_32X_Active)
        lstrcpy(text, "32X inactive - write rate: --");
    else if (!count)
        lstrcpy(text, "Waiting for a completed frame...");
    else
        sprintf(text, "Average (up to 1 emulated second): %.1f KiB/s%s",
            VRAM32XTransfer_GetBytesPerSecond() / 1024.0, Paused ? " / Paused" : "");
    GetDlgItemText(transfer_window, IDC_VRAM32X_TRANSFER_RATE, previous, sizeof(previous));
    if (strcmp(text, previous)) SetDlgItemText(transfer_window, IDC_VRAM32X_TRANSFER_RATE, text);
    InvalidateRect(GetDlgItem(transfer_window, IDC_VRAM32X_TRANSFER_GRAPH), NULL, FALSE);
}

static INT_PTR CALLBACK TransferProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        transfer_window = hwnd;
        HandleWindow_KMod[DMODE_VRAM32X_TRANSFER - 1] = hwnd;
        SetTimer(hwnd, 1, 100, NULL);
        vram32xtransfer_update();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) vram32xtransfer_update();
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_VRAM32X_TRANSFER_GRAPH) {
            unsigned long values[VRAM32X_TRANSFER_HISTORY], scale = 4096;
            unsigned int i, count = (_32X_Started || CD_32X_Active) ? VRAM32XTransfer_GetHistory(values) : 0;
            char latest[80], top[32], middle[32];
            for (i = 0; i < count; ++i)
                while (scale < values[i] && scale <= 0x7fffffffUL) scale *= 2;
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
        CloseWindow_KMod(DMODE_VRAM32X_TRANSFER);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        transfer_window = NULL;
        HandleWindow_KMod[DMODE_VRAM32X_TRANSFER - 1] = NULL;
        OpenedWindow_KMod[DMODE_VRAM32X_TRANSFER - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void vram32xtransfer_create(HINSTANCE instance, HWND parent)
{
    transfer_window = CreateDialog(instance, MAKEINTRESOURCE(IDD_VRAM32X_TRANSFER), parent, TransferProc);
}

void vram32xtransfer_show(BOOL visible)
{
    if (!transfer_window) return;
    if (visible) vram32xtransfer_update();
    ShowWindow(transfer_window, visible ? SW_SHOW : SW_HIDE);
}

void vram32xtransfer_destroy(void)
{
    if (transfer_window) DestroyWindow(transfer_window);
}

void vram32xtransfer_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "VRAM32XTransferOpen",
        OpenedWindow_KMod[DMODE_VRAM32X_TRANSFER - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(transfer_window, "VRAM32XTransferRect", config);
}

void vram32xtransfer_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "VRAM32XTransferOpen", 0, config) != 0;
    if (!transfer_window) return;
    GetWindowRect(transfer_window, &rect);
    DebugWindow_RestoreGeometry(transfer_window, "VRAM32XTransferRect", config);
    SetWindowPos(transfer_window, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_VRAM32X_TRANSFER - 1] = visible;
    vram32xtransfer_show(visible);
}
