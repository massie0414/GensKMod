#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "../gens.h"
#include "../resource.h"
#include "../mem_sh2.h"
#include "../vdp_32X.h"
#include "../vdp_io.h"
#include "common.h"
#include "window_geometry.h"
#include "memory_32x.h"

typedef struct {
    const char *name;
    unsigned char *data;
    unsigned base, size, swap;
    BOOL writable;
} MemoryRegion;
/* SDRAM/ROM/BIOS are stored in SH2 byte order. Framebuffers contain
   little-endian words; expose their bytes in SH2 (big-endian) order.
   Inspect backing memory directly: bus handlers change cycle/IO state. */
enum { REGION_PALETTE = 4, REGION_LINE0, REGION_LINE1, REGION_SH2_FB, REGION_SH2_OVERWRITE };
static MemoryRegion regions[] = {
    {"SDRAM", _32X_Ram, 0x06000000, sizeof(_32X_Ram), 0, TRUE},
    {"ROM (read only)", _32X_Rom, 0x02000000, sizeof(_32X_Rom), 0, FALSE},
    {"Master BIOS (read only)", _32X_MSH2_Rom, 0, sizeof(_32X_MSH2_Rom), 0, FALSE},
    {"Slave BIOS (read only)", _32X_SSH2_Rom, 0, sizeof(_32X_SSH2_Rom), 0, FALSE},
    {"Color palette", (unsigned char *)_32X_VDP_CRam, 0x20004200, sizeof(_32X_VDP_CRam), 1, TRUE},
    /* Line tables show the first 256 words of each SH2 address window. */
    {"Line table - Framebuffer", NULL, 0x04000000, 0x200, 1, FALSE},
    {"Line table - Overwrite", NULL, 0x04020000, 0x200, 1, FALSE},
    /* Address views follow the current mapping. */
    {"VRAM Framebuffer", NULL, 0x04000000, 0x20000, 1, FALSE},
    {"VRAM Overwrite", NULL, 0x04020000, 0x20000, 1, FALSE}
};
static HWND memoryWindow, grid;
static unsigned regionIndex, selected;
static int fixedWidth, minimumHeight, initialClientHeight;
static RECT initialGrid, initialStatus;

static void InitLayout(HWND hwnd) {
    RECT outer, client, minimum = {0, 0, 0, 150};
    GetWindowRect(hwnd, &outer);
    GetClientRect(hwnd, &client);
    fixedWidth = outer.right - outer.left;
    initialClientHeight = client.bottom;
    MapDialogRect(hwnd, &minimum);
    minimumHeight = minimum.bottom + (outer.bottom - outer.top) - client.bottom;
    GetWindowRect(grid, &initialGrid);
    MapWindowPoints(NULL, hwnd, (POINT *)&initialGrid, 2);
    GetWindowRect(GetDlgItem(hwnd, IDC_MEMORY_STATUS), &initialStatus);
    MapWindowPoints(NULL, hwnd, (POINT *)&initialStatus, 2);
}
static void ResizeLayout(HWND hwnd) {
    RECT client;
    int delta;
    GetClientRect(hwnd, &client);
    delta = client.bottom - initialClientHeight;
    MoveWindow(grid, initialGrid.left, initialGrid.top,
        initialGrid.right - initialGrid.left,
        max(1, initialGrid.bottom - initialGrid.top + delta), TRUE);
    MoveWindow(GetDlgItem(hwnd, IDC_MEMORY_STATUS), initialStatus.left,
        initialStatus.top + delta, initialStatus.right - initialStatus.left,
        initialStatus.bottom - initialStatus.top, TRUE);
}
static BOOL Active(void) { return _32X_Started || CD_32X_Active; }
static BOOL AddressView(void) {
    return regionIndex == REGION_SH2_FB || regionIndex == REGION_SH2_OVERWRITE ||
        regionIndex == REGION_LINE0 || regionIndex == REGION_LINE1;
}
static BOOL Readable(void) { return Active() && (!AddressView() || _32X_FM); }
static unsigned ReadByte(unsigned offset) {
    MemoryRegion *r = &regions[regionIndex];
    unsigned char *data = r->data;
    if (AddressView()) {
        /* Use the actual display bit, not the pending swap request. Both
           address windows read the drawing buffer without bus side effects. */
        data = _32X_VDP_Ram + ((_32X_VDP.State & 1) ? 0 : 0x20000);
    }
    return data[offset ^ r->swap];
}
static BOOL ParseHex(const char *text, unsigned *value) {
    unsigned n = 0, digits = 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
    while (*text) {
        unsigned d;
        char c = *text++;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return FALSE;
        if (++digits > 8) return FALSE;
        n = (n << 4) | d;
    }
    if (!digits) return FALSE;
    *value = n; return TRUE;
}
static void Refresh(void) {
    char status[256], detail[160] = "";
    MemoryRegion *r = &regions[regionIndex];
    EnableWindow(GetDlgItem(memoryWindow, IDC_MEMORY_WRITE), Active() && r->writable);
    if (Active() && AddressView() && regionIndex != REGION_LINE0 && regionIndex != REGION_LINE1) {
        sprintf(detail, " Displayed: FB%u | Mapped: FB%u | %s",
            _32X_VDP.State & 1, (_32X_VDP.State & 1) ^ 1,
            _32X_FM ? "Both windows read the same buffer." : "SH2 access disabled (FM=0).");
    } else if (Active() && regionIndex == REGION_PALETTE) {
        unsigned word = _32X_VDP_CRam[selected / 2];
        sprintf(detail, " Color %u: %04X | R:%u G:%u B:%u (0-31) | Priority:%u",
            selected / 2, word, word & 31, (word >> 5) & 31, (word >> 10) & 31, word >> 15);
    } else if (Readable() && (regionIndex == REGION_LINE0 || regionIndex == REGION_LINE1)) {
        unsigned offset = selected & ~1U;
        unsigned word = (ReadByte(offset) << 8) | ReadByte(offset + 1);
        sprintf(detail, " Line %u: %04X words -> buffer byte offset %05X",
            selected / 2, word, word * 2);
    } else if (Active() && AddressView() && !_32X_FM) {
        strcpy(detail, " SH2 access disabled (FM=0).");

    }
    sprintf(status, "%s | %08X - %08X | %s%s",
        r->name, r->base, r->base + r->size - 1,
        Active() ? (r->writable ? "Select a hex byte, then Write." : "Read only.") : "32X inactive.",
        detail);
    SetDlgItemText(memoryWindow, IDC_MEMORY_STATUS, status);
    InvalidateRect(grid, NULL, FALSE);
}
static void SelectByte(unsigned offset) {
    char text[32];
    selected = offset;
    sprintf(text, "%08X", regions[regionIndex].base + selected);
    SetDlgItemText(memoryWindow, IDC_MEMORY_ADDRESS, text);
    if (Readable()) sprintf(text, "%02X", ReadByte(selected));
    else text[0] = 0;
    SetDlgItemText(memoryWindow, IDC_MEMORY_VALUE, text);
    ListView_SetItemState(grid, selected / 16, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(grid, selected / 16, FALSE);
    Refresh();
}
static void WriteByte(void) {
    char text[32]; unsigned value, address;
    MemoryRegion *r = &regions[regionIndex];
    if (!Active() || !r->writable) return;
    GetDlgItemText(memoryWindow, IDC_MEMORY_ADDRESS, text, sizeof(text));
    if (!ParseHex(text, &address) || address < r->base || address - r->base >= r->size) {
        MessageBox(memoryWindow, "Enter an address within the selected region.", "32X - Memory Editor", MB_OK); return;
    }
    GetDlgItemText(memoryWindow, IDC_MEMORY_VALUE, text, sizeof(text));
    if (!ParseHex(text, &value) || value > 255) {
        MessageBox(memoryWindow, "Enter a hex byte from 00 to FF.", "32X - Memory Editor", MB_OK); return;
    }
    /* Runs on the emulation/UI thread, including while paused. Raw framebuffer
       edits intentionally bypass the hardware transparent-write behavior. */
    r->data[(address - r->base) ^ r->swap] = (unsigned char)value;
    if (regionIndex == REGION_PALETTE) {
        /* The renderer caches converted colors, just like the SH2 palette
           word-write handler. A debugger byte edit must update that cache. */
        unsigned index = (address - r->base) / 2;
        _32X_VDP_CRam_Ajusted[index] = _32X_Palette_16B[_32X_VDP_CRam[index]];
    }
    SelectByte(address - r->base);
    Refresh();
}
static INT_PTR CALLBACK MemoryProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_INITDIALOG: {
        int i; LVCOLUMN col = {0}; char label[16]; RECT rect;
        memoryWindow = hwnd;
        HandleWindow_KMod[DMODE_32_MEMORY - 1] = hwnd;
        grid = GetDlgItem(hwnd, IDC_MEMORY_GRID);
        InitLayout(hwnd);
        SendMessage(grid, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), TRUE);
        ListView_SetExtendedListViewStyle(grid, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
        GetClientRect(grid, &rect);
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        for (i = 0; i < 18; ++i) {
            if (!i) strcpy(label, "Address");
            else if (i == 17) strcpy(label, "ASCII");
            else sprintf(label, "%X", i - 1);
            col.pszText = label;
            col.cx = i == 0 ? 82 : (i == 17 ? 140 : (rect.right - 244) / 16);
            ListView_InsertColumn(grid, i, &col);
        }
        for (i = 0; i < sizeof(regions)/sizeof(regions[0]); ++i)
            SendDlgItemMessage(hwnd, IDC_MEMORY_REGION, CB_ADDSTRING, 0, (LPARAM)regions[i].name);
        SendDlgItemMessage(hwnd, IDC_MEMORY_REGION, CB_SETDROPPEDWIDTH, 300, 0);
        SendDlgItemMessage(hwnd, IDC_MEMORY_REGION, CB_SETCURSEL, 0, 0);
        SendDlgItemMessage(hwnd, IDC_MEMORY_ADDRESS, EM_SETLIMITTEXT, 10, 0);
        SendDlgItemMessage(hwnd, IDC_MEMORY_VALUE, EM_SETLIMITTEXT, 4, 0);
        ListView_SetItemCount(grid, regions[0].size / 16);
        SelectByte(0); Refresh(); SetTimer(hwnd, 1, 250, NULL);
        return TRUE;
    }
    case WM_GETMINMAXINFO:
        if (fixedWidth) {
            MINMAXINFO *limits = (MINMAXINFO *)lp;
            limits->ptMinTrackSize.x = fixedWidth;
            limits->ptMaxTrackSize.x = fixedWidth;
            limits->ptMinTrackSize.y = minimumHeight;
        }
        return TRUE;
    case WM_SIZE:
        if (grid && wp != SIZE_MINIMIZED) ResizeLayout(hwnd);
        return TRUE;
    case WM_NOTIFY: {
        NMHDR *hdr = (NMHDR *)lp;
        if (hdr->idFrom != IDC_MEMORY_GRID) break;
        if (hdr->code == LVN_GETDISPINFO) {
            NMLVDISPINFO *info = (NMLVDISPINFO *)lp;
            static char text[32]; unsigned offset = info->item.iItem * 16, i;
            int sub = info->item.iSubItem;
            if (!(info->item.mask & LVIF_TEXT) || offset >= regions[regionIndex].size) return TRUE;
            if (!sub) sprintf(text, "%08X", regions[regionIndex].base + offset);
            else if (!Readable()) strcpy(text, sub == 17 ? "" : "--");
            else if (sub <= 16) sprintf(text, "%02X", ReadByte(offset + sub - 1));
            else {
                for (i = 0; i < 16; ++i) { unsigned c = ReadByte(offset + i); text[i] = (c >= 32 && c <= 126) ? (char)c : '.'; }
                text[16] = 0;
            }
            info->item.pszText = text; return TRUE;
        }
        if (hdr->code == NM_CLICK) {
            NMITEMACTIVATE *item = (NMITEMACTIVATE *)lp;
            if (item->iItem >= 0) SelectByte(item->iItem * 16 + ((item->iSubItem >= 1 && item->iSubItem <= 16) ? item->iSubItem - 1 : 0));
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_MEMORY_REGION:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                regionIndex = (unsigned)SendDlgItemMessage(hwnd, IDC_MEMORY_REGION, CB_GETCURSEL, 0, 0);
                ListView_SetItemCount(grid, regions[regionIndex].size / 16);
                SelectByte(0); Refresh();
            } return TRUE;
        case IDC_MEMORY_GOTO: {
            char text[32]; unsigned address; MemoryRegion *r = &regions[regionIndex];
            GetDlgItemText(hwnd, IDC_MEMORY_ADDRESS, text, sizeof(text));
            if (ParseHex(text, &address) && address >= r->base && address - r->base < r->size) SelectByte(address - r->base);
            else MessageBox(hwnd, "Enter an address within the selected region.", "32X - Memory Editor", MB_OK);
            return TRUE;
        }
        case IDC_MEMORY_WRITE: WriteByte(); return TRUE;
        case IDCANCEL: CloseWindow_KMod(DMODE_32_MEMORY); return TRUE;
        } break;
    case WM_TIMER: if (IsWindowVisible(hwnd)) Refresh(); return TRUE;
    case WM_CLOSE: CloseWindow_KMod(DMODE_32_MEMORY); return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1); memoryWindow = grid = NULL; fixedWidth = 0;
        HandleWindow_KMod[DMODE_32_MEMORY - 1] = NULL;
        OpenedWindow_KMod[DMODE_32_MEMORY - 1] = FALSE; return TRUE;
    }
    return FALSE;
}
void memory32x_create(HINSTANCE instance, HWND parent) {
    CreateDialog(instance, MAKEINTRESOURCE(IDD_DEBUG32X_MEMORY), parent, MemoryProc);
}
void memory32x_show(BOOL visible) {
    if (visible) Refresh();
    ShowWindow(memoryWindow, visible ? SW_SHOW : SW_HIDE);
}
void memory32x_save_window(const char *config) {
    WritePrivateProfileString("DebugWindows", "32XMemoryOpen", OpenedWindow_KMod[DMODE_32_MEMORY - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(memoryWindow, "32XMemoryRect", config);
}
void memory32x_restore_window(const char *config) {
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "32XMemoryOpen", 0, config) != 0;
    DebugWindow_RestoreGeometry(memoryWindow, "32XMemoryRect", config);
    GetWindowRect(memoryWindow, &rect);
    SetWindowPos(memoryWindow, NULL, 0, 0, fixedWidth, max(minimumHeight, rect.bottom-rect.top), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_32_MEMORY - 1] = visible && memoryWindow != NULL;
    memory32x_show(visible);
}
