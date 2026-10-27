#include <windows.h>
#include <stdio.h>
#include "../gens.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "../vram_transfer.h"
#include "common.h"
#include "vdp_transfer_panel.h"
#include "frame_graph.h"
#include "window_geometry.h"

static HWND windows[VDP_TRANSFER_KINDS];
static const char *names[] = {"", "CRAM", "VSRAM", "VDP DMA"};
static const char *keys[] = {"", "CRAMTransfer", "VSRAMTransfer", "VDPDMATransfer"};
static int WindowMode(int kind) { return DMODE_CRAM_TRANSFER + kind - 1; }

static unsigned long GraphScale(int kind)
{
    unsigned long values[VRAM_TRANSFER_HISTORY], scale = 4096;
    unsigned int count, i;
    int write;
    if (!Genesis_Started && !SegaCD_Started && !_32X_Started && !CD_32X_Active) return scale;
    for (write = 0; write < 2; ++write) {
        count = VDPTransfer_GetHistory(kind, write, values);
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
    unsigned long values[VRAM_TRANSFER_HISTORY];
    unsigned int count;
    int write, active = Genesis_Started || SegaCD_Started || _32X_Started || CD_32X_Active;
    if (!transfer_window) return;
    count = active ? VDPTransfer_GetHistory(kind, VRAM_TRANSFER_READ, values) : 0;
    for (write = 0; write < 2; ++write) {
        if (count)
            sprintf(text, "Average (up to 1 emulated second): %.1f KiB/s",
                VDPTransfer_GetBytesPerSecond(kind, write) / 1024.0);
        else lstrcpy(text, "Average: --");
        SetTextIfChanged(transfer_window, write ? IDC_VDP_WRITE_RATE : IDC_VDP_READ_RATE, text);
        InvalidateRect(GetDlgItem(transfer_window,
            write ? IDC_VDP_WRITE_GRAPH : IDC_VDP_READ_GRAPH), NULL, FALSE);
    }
    if (!active) lstrcpy(text, "Emulation inactive");
    else if (!count) lstrcpy(text, "Waiting for a completed frame...");
    else sprintf(text, "%s%s", names[kind], Paused ? " / Paused (history retained)" : "");
    SetTextIfChanged(transfer_window, IDC_VDP_TRANSFER_STATUS, text);
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
            sprintf(title, "Genesis - %s Transfer", names[kind]);
            SetWindowText(hwnd, title);
            sprintf(label, "Read from %s (KiB per frame)", kind == VDP_TRANSFER_DMA ? "DMA source" : names[kind]);
            SetDlgItemText(hwnd, IDC_VDP_READ_GROUP, label);
            sprintf(label, "Write to %s (KiB per frame)", kind == VDP_TRANSFER_DMA ? "DMA destination" : names[kind]);
            SetDlgItemText(hwnd, IDC_VDP_WRITE_GROUP, label);
            SetDlgItemText(hwnd, IDC_VDP_TRANSFER_SCOPE, kind == VDP_TRANSFER_DMA ?
                "VDP engine / fill writes only / CPU copies excluded" :
                "Main-CPU data accesses / VDP DMA excluded");
        }
        HandleWindow_KMod[WindowMode(kind) - 1] = hwnd;
        SetTimer(hwnd, 1, 100, NULL);
        UpdatePanel(kind);
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) UpdatePanel(kind);
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_VDP_READ_GRAPH || wParam == IDC_VDP_WRITE_GRAPH) {
            unsigned long values[VRAM_TRANSFER_HISTORY], scale = GraphScale(kind);
            int write = wParam == IDC_VDP_WRITE_GRAPH;
            unsigned int count = (Genesis_Started || SegaCD_Started || _32X_Started || CD_32X_Active) ? VDPTransfer_GetHistory(kind, write, values) : 0;
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

void vdptransfer_create(HINSTANCE instance, HWND parent)
{
    int kind;
    for (kind = VDP_TRANSFER_CRAM; kind <= VDP_TRANSFER_DMA; ++kind)
        CreateDialogParam(instance, MAKEINTRESOURCE(IDD_VDP_TRANSFER), parent, TransferProc, kind);
}

void vdptransfer_update(void)
{
    int kind;
    for (kind = VDP_TRANSFER_CRAM; kind <= VDP_TRANSFER_DMA; ++kind) UpdatePanel(kind);
}

void vdptransfer_show(int kind, BOOL visible)
{
    if (!windows[kind]) return;
    if (visible) UpdatePanel(kind);
    ShowWindow(windows[kind], visible ? SW_SHOW : SW_HIDE);
}

void vdptransfer_destroy(void)
{
    int kind;
    for (kind = VDP_TRANSFER_CRAM; kind <= VDP_TRANSFER_DMA; ++kind)
        if (windows[kind]) DestroyWindow(windows[kind]);
}

void vdptransfer_save_window(const char *config)
{
    int kind;
    char key[64];
    for (kind = VDP_TRANSFER_CRAM; kind <= VDP_TRANSFER_DMA; ++kind) {
        sprintf(key, "%sOpen", keys[kind]);
        WritePrivateProfileString("DebugWindows", key,
            OpenedWindow_KMod[WindowMode(kind) - 1] ? "1" : "0", config);
        sprintf(key, "%sRect", keys[kind]);
        DebugWindow_SaveGeometry(windows[kind], key, config);
    }
}

void vdptransfer_restore_window(const char *config)
{
    int kind;
    char key[64];
    for (kind = VDP_TRANSFER_CRAM; kind <= VDP_TRANSFER_DMA; ++kind) {
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
        vdptransfer_show(kind, visible);
    }
}
