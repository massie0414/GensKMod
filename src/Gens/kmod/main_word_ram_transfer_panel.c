#include <windows.h>
#include <stdio.h>
#include "../gens.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "../main_word_ram_transfer.h"
#include "../Mem_S68K.h"
#include "common.h"
#include "main_word_ram_transfer_panel.h"
#include "frame_graph.h"
#include "window_geometry.h"

static HWND transfer_window;

static unsigned long GraphScale(void)
{
    unsigned long values[MAIN_WORD_RAM_TRANSFER_HISTORY], scale = 4096;
    unsigned int count, i;
    int write;
    if (!SegaCD_Started && !CD_32X_Active) return scale;
    for (write = 0; write < 2; ++write) {
        count = MainWordRAMTransfer_GetHistory(write, values);
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

void mainwordramtransfer_update(void)
{
    char text[160];
    unsigned long values[MAIN_WORD_RAM_TRANSFER_HISTORY];
    unsigned int count;
    int write, active = SegaCD_Started || CD_32X_Active;
    if (!transfer_window) return;
    count = active ? MainWordRAMTransfer_GetHistory(MAIN_WORD_RAM_TRANSFER_READ, values) : 0;
    for (write = 0; write < 2; ++write) {
        if (count)
            sprintf(text, "Average (up to 1 emulated second): %.1f KiB/s",
                MainWordRAMTransfer_GetBytesPerSecond(write) / 1024.0);
        else lstrcpy(text, "Average: --");
        SetTextIfChanged(write ? IDC_MAIN_WORD_RAM_WRITE_RATE : IDC_MAIN_WORD_RAM_READ_RATE, text);
        InvalidateRect(GetDlgItem(transfer_window,
            write ? IDC_MAIN_WORD_RAM_WRITE_GRAPH : IDC_MAIN_WORD_RAM_READ_GRAPH), NULL, FALSE);
    }
    if (!active) lstrcpy(text, "SegaCD inactive");
    else if (!count) lstrcpy(text, "Waiting for a completed frame...");
    else if (Ram_Word_State & 2)
        sprintf(text, "1M mode / Main-CPU bank %u%s", Ram_Word_State & 1,
            Paused ? " / Paused (history retained)" : "");
    else sprintf(text, "2M mode%s", Paused ? " / Paused (history retained)" : "");
    SetTextIfChanged(IDC_MAIN_WORD_RAM_TRANSFER_STATUS, text);
}

static INT_PTR CALLBACK TransferProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        transfer_window = hwnd;
        HandleWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER - 1] = hwnd;
        SetTimer(hwnd, 1, 100, NULL);
        mainwordramtransfer_update();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) mainwordramtransfer_update();
        return TRUE;
    case WM_DRAWITEM:
        if (wParam == IDC_MAIN_WORD_RAM_READ_GRAPH || wParam == IDC_MAIN_WORD_RAM_WRITE_GRAPH) {
            unsigned long values[MAIN_WORD_RAM_TRANSFER_HISTORY], scale = GraphScale();
            int write = wParam == IDC_MAIN_WORD_RAM_WRITE_GRAPH;
            unsigned int count = (SegaCD_Started || CD_32X_Active) ? MainWordRAMTransfer_GetHistory(write, values) : 0;
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
        CloseWindow_KMod(DMODE_MAIN_WORD_RAM_TRANSFER);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        transfer_window = NULL;
        HandleWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER - 1] = NULL;
        OpenedWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void mainwordramtransfer_create(HINSTANCE instance, HWND parent)
{
    transfer_window = CreateDialog(instance, MAKEINTRESOURCE(IDD_MAIN_WORD_RAM_TRANSFER), parent, TransferProc);
}

void mainwordramtransfer_show(BOOL visible)
{
    if (!transfer_window) return;
    if (visible) mainwordramtransfer_update();
    ShowWindow(transfer_window, visible ? SW_SHOW : SW_HIDE);
}

void mainwordramtransfer_destroy(void)
{
    if (transfer_window) DestroyWindow(transfer_window);
}

void mainwordramtransfer_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "MainWordRAMTransferOpen",
        OpenedWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(transfer_window, "MainWordRAMTransferRect", config);
}

void mainwordramtransfer_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "MainWordRAMTransferOpen", 0, config) != 0;
    if (!transfer_window) return;
    GetWindowRect(transfer_window, &rect);
    DebugWindow_RestoreGeometry(transfer_window, "MainWordRAMTransferRect", config);
    SetWindowPos(transfer_window, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER - 1] = visible;
    mainwordramtransfer_show(visible);
}
