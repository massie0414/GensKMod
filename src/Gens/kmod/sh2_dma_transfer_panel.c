#include <windows.h>
#include <stdio.h>
#include "../gens.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "../sh2_dma_transfer.h"
#include "common.h"
#include "sh2_dma_transfer_panel.h"
#include "frame_graph.h"
#include "window_geometry.h"

static HWND windows[SH2_DMA_TRANSFER_KINDS];
static const char *names[] = {"Master SH2 / DMA channels 0 + 1", "Slave SH2 / DMA channels 0 + 1"};
static const char *keys[] = {"MasterSH2DMATransfer", "SlaveSH2DMATransfer"};
static int WindowMode(int kind) { return DMODE_MASTER_SH2_DMA_TRANSFER + kind; }

static unsigned long GraphScale(int kind)
{
    unsigned long values[SH2_DMA_TRANSFER_HISTORY], scale = 4096;
    unsigned int count, i;
    int write;
    if (!_32X_Started && !CD_32X_Active) return scale;
    for (write = 0; write < 2; ++write) {
        count = SH2DMATransfer_GetHistory(kind, write, values);
        for (i = 0; i < count; ++i)
            while (scale < values[i] && scale <= 0x7fffffffUL) scale *= 2;
    }
    return scale;
}

static void SetTextIfChanged(HWND transfer_window, int id, const char *text)
{
    char previous[160];
    GetDlgItemText(transfer_window, id, previous, sizeof(previous));
    if (strcmp(text, previous)) SetDlgItemText(transfer_window, id, text);
}

static void UpdatePanel(int kind)
{
    HWND transfer_window = windows[kind];
    char text[160];
    unsigned long values[SH2_DMA_TRANSFER_HISTORY];
    unsigned int count;
    int write, active = _32X_Started || CD_32X_Active;
    if (!transfer_window) return;
    count = active ? SH2DMATransfer_GetHistory(kind, SH2_DMA_TRANSFER_READ, values) : 0;
    for (write = 0; write < 2; ++write) {
        if (count)
            sprintf(text, "Average (up to 1 emulated second): %.1f KiB/s",
                SH2DMATransfer_GetBytesPerSecond(kind, write) / 1024.0);
        else lstrcpy(text, "Average: --");
        SetTextIfChanged(transfer_window, write ? IDC_SH2_DMA_WRITE_RATE : IDC_SH2_DMA_READ_RATE, text);
        InvalidateRect(GetDlgItem(transfer_window,
            write ? IDC_SH2_DMA_WRITE_GRAPH : IDC_SH2_DMA_READ_GRAPH), NULL, FALSE);
    }
    if (!active) lstrcpy(text, "Emulation inactive");
    else if (!count) lstrcpy(text, "Waiting for a completed frame...");
    else sprintf(text, "%s%s", names[kind], Paused ? " / Paused (history retained)" : "");
    SetTextIfChanged(transfer_window, IDC_SH2_DMA_TRANSFER_STATUS, text);
}

static INT_PTR CALLBACK TransferProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int kind = (int)GetWindowLongPtr(hwnd, DWLP_USER);
    switch (message) {
    case WM_INITDIALOG:
        kind = (int)lParam;
        SetWindowLongPtr(hwnd, DWLP_USER, kind);
        windows[kind] = hwnd;
        {
            char title[100], label[100];
            sprintf(title, "32X - %s SH2 DMA Transfer", kind ? "Slave" : "Master");
            SetWindowText(hwnd, title);
            sprintf(label, "Read from DMA source (KiB per frame)");
            SetDlgItemText(hwnd, IDC_SH2_DMA_READ_GROUP, label);
            sprintf(label, "Write to DMA destination (KiB per frame)");
            SetDlgItemText(hwnd, IDC_SH2_DMA_WRITE_GROUP, label);
            SetDlgItemText(hwnd, IDC_SH2_DMA_TRANSFER_SCOPE, "DMA payload only / ordinary CPU accesses excluded");
        }
        HandleWindow_KMod[WindowMode(kind) - 1] = hwnd;
        SetTimer(hwnd, 1, 100, NULL);
        UpdatePanel(kind);
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) UpdatePanel(kind);
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_SH2_DMA_READ_GRAPH || wParam == IDC_SH2_DMA_WRITE_GRAPH) {
            unsigned long values[SH2_DMA_TRANSFER_HISTORY], scale = GraphScale(kind);
            int write = wParam == IDC_SH2_DMA_WRITE_GRAPH;
            unsigned int count = (_32X_Started || CD_32X_Active) ? SH2DMATransfer_GetHistory(kind, write, values) : 0;
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
        CloseWindow_KMod(WindowMode(kind));
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        windows[kind] = NULL;
        HandleWindow_KMod[WindowMode(kind) - 1] = NULL;
        OpenedWindow_KMod[WindowMode(kind) - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void sh2dmatransfer_create(HINSTANCE instance, HWND parent)
{
    int kind;
    for (kind = SH2_DMA_MASTER; kind <= SH2_DMA_SLAVE; ++kind)
        CreateDialogParam(instance, MAKEINTRESOURCE(IDD_SH2_DMA_TRANSFER), parent, TransferProc, kind);
}

void sh2dmatransfer_update(void)
{
    int kind;
    for (kind = SH2_DMA_MASTER; kind <= SH2_DMA_SLAVE; ++kind) UpdatePanel(kind);
}

void sh2dmatransfer_show(int kind, BOOL visible)
{
    if (!windows[kind]) return;
    if (visible) UpdatePanel(kind);
    ShowWindow(windows[kind], visible ? SW_SHOW : SW_HIDE);
}

void sh2dmatransfer_destroy(void)
{
    int kind;
    for (kind = SH2_DMA_MASTER; kind <= SH2_DMA_SLAVE; ++kind)
        if (windows[kind]) DestroyWindow(windows[kind]);
}

void sh2dmatransfer_save_window(const char *config)
{
    int kind;
    char key[64];
    for (kind = SH2_DMA_MASTER; kind <= SH2_DMA_SLAVE; ++kind) {
        sprintf(key, "%sOpen", keys[kind]);
        WritePrivateProfileString("DebugWindows", key,
            OpenedWindow_KMod[WindowMode(kind) - 1] ? "1" : "0", config);
        sprintf(key, "%sRect", keys[kind]);
        DebugWindow_SaveGeometry(windows[kind], key, config);
    }
}

void sh2dmatransfer_restore_window(const char *config)
{
    int kind;
    char key[64];
    for (kind = SH2_DMA_MASTER; kind <= SH2_DMA_SLAVE; ++kind) {
        RECT rect;
        BOOL visible;
        if (!windows[kind]) continue;
        sprintf(key, "%sOpen", keys[kind]);
        visible = GetPrivateProfileInt("DebugWindows", key, 0, config) != 0;
        GetWindowRect(windows[kind], &rect);
        sprintf(key, "%sRect", keys[kind]);
        DebugWindow_RestoreGeometry(windows[kind], key, config);
        SetWindowPos(windows[kind], NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        OpenedWindow_KMod[WindowMode(kind) - 1] = visible;
        sh2dmatransfer_show(kind, visible);
    }
}
