#include <windows.h>
#include <richedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "../gens.h"
#include "../vdp_io.h"
#include "../resource.h"
#include "../Mem_M68k.h"
#include "../M68KD.h"
#include "../SH2D.h"
#include "../SH2.h"
#include "../Mem_SH2.h"
#include "../Star_68k.h"
#include "../Mem_S68k.h"
#include "../Mem_Z80.h"
#include "../Z80.h"
#include "../Z80dis.h"
#include "disassembly.h"
#include "window_geometry.h"

/* Decode instruction backing storage only; never call emulated bus handlers. */
static HWND window;
static HFONT font;
static unsigned cursor, page, nextPage, limit;
static unsigned history[256], historyCount;
static BOOL sh2, truncated;
static BOOL slave, sub68k, z80;
static unsigned char *pcData;
static unsigned pcSize, displayBase, pcSwap;
static HMODULE richEdit;
static unsigned lineAddresses[256], lineCount;
static int highlightedLine = -1;

static BOOL CPUActive(void)
{
    return Game && (sub68k ? SegaCD_Started : sh2 ? _32X_Started :
        (Genesis_Started || _32X_Started || SegaCD_Started));
}

static unsigned CurrentPC(void)
{
    if (z80) return z80_Get_PC(&M_Z80) & 0xFFFF;
    if (sh2) return SH2_Get_PC(slave ? &S_SH2 : &M_SH2);
    return (sub68k ? sub68k_context.pc : main68k_context.pc) & 0xFFFFFF;
}

static void SetLineBackground(HWND text, int line, COLORREF color)
{
    CHARFORMAT2A format = {0};
    CHARRANGE range;
    range.cpMin = (LONG)SendMessage(text, EM_LINEINDEX, line, 0);
    range.cpMax = range.cpMin + (LONG)SendMessage(text, EM_LINELENGTH, range.cpMin, 0);
    format.cbSize = sizeof(format);
    format.dwMask = CFM_BACKCOLOR | CFM_COLOR;
    format.crBackColor = color;
    format.crTextColor = RGB(0, 0, 0);
    SendMessage(text, EM_EXSETSEL, 0, (LPARAM)&range);
    SendMessage(text, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
}

static void HighlightPC(void)
{
    unsigned pc, i;
    int current = -1;
    HWND text;
    CHARRANGE selection;
    POINT scroll;
    if (!window) return;
    if (CPUActive())
    {
        pc = CurrentPC();
        for (i = 0; i < lineCount; ++i)
            if (sh2 ? ((pc & 0xDFFFFFFF) == (lineAddresses[i] & 0xDFFFFFFF)) : pc == lineAddresses[i])
            { current = (int)i; break; }
    }
    if (current == highlightedLine) return;
    text = GetDlgItem(window, IDC_DISASM_TEXT);
    SendMessage(text, EM_EXGETSEL, 0, (LPARAM)&selection);
    SendMessage(text, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    SendMessage(text, WM_SETREDRAW, FALSE, 0);
    if (highlightedLine >= 0) SetLineBackground(text, highlightedLine, RGB(255, 255, 255));
    if (current >= 0) SetLineBackground(text, current, RGB(255, 255, 128));
    highlightedLine = current;
    SendMessage(text, EM_EXSETSEL, 0, (LPARAM)&selection);
    SendMessage(text, EM_SETSCROLLPOS, 0, (LPARAM)&scroll);
    SendMessage(text, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(text, NULL, FALSE);
}

static void SetDisassemblyText(const char *value)
{
    CHARFORMAT2A format = {0};
    HWND text = GetDlgItem(window, IDC_DISASM_TEXT);
    CHARRANGE all = {0, -1}, caret = {0, 0};
    SetWindowText(text, value);
    format.cbSize = sizeof(format);
    format.dwMask = CFM_BACKCOLOR | CFM_COLOR;
    format.crBackColor = RGB(255, 255, 255);
    format.crTextColor = RGB(0, 0, 0);
    SendMessage(text, EM_EXSETSEL, 0, (LPARAM)&all);
    SendMessage(text, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    SendMessage(text, EM_EXSETSEL, 0, (LPARAM)&caret);
    highlightedLine = -1;
}

static void UseROM(void)
{
    pcData = NULL;
    displayBase = sh2 ? 0x02000000 : 0;
    if (sub68k || z80)
    {
        pcData = sub68k ? Ram_Prg : Ram_Z80;
        pcSize = sub68k ? sizeof(Ram_Prg) : sizeof(Ram_Z80);
        pcSwap = sub68k ? 1 : 0;
    }
}

static unsigned char RomByte(unsigned address)
{
    /* The loaded 68000 ROM is stored with each word byte-swapped. */
    unsigned physical;
    if (pcData)
    {
        physical = address ^ pcSwap;
        return physical < pcSize ? pcData[physical] : 0;
    }
    physical = address ^ 1;
    return physical < sizeof(Rom_Data) ? Rom_Data[physical] : 0;
}

static unsigned short NextWord(void)
{
    unsigned short value = 0;
    if (cursor < limit && limit - cursor >= 2)
        value = (unsigned short)((RomByte(cursor) << 8) | RomByte(cursor + 1));
    else truncated = TRUE;
    cursor += 2;
    return value;
}

static unsigned int NextLong(void)
{
    unsigned int high = NextWord();
    return (high << 16) | NextWord();
}

static void Render(void)
{
    char text[65536], address[32], instruction[256], bytes[64], status[256];
    unsigned start, i, line, used = 0, count;
    if (!window) return;
    lineCount = 0;
    limit = CPUActive() ?
        (pcData ? pcSize : min(Rom_Size, sizeof(Rom_Data))) : 0;
    if (!limit)
    {
        SetDisassemblyText(sub68k ? "The Sega CD 68000 CPU is inactive." : "No cartridge ROM loaded.");
        SetDlgItemText(window, IDC_DISASM_STATUS, "Disassembly - read only");
        EnableWindow(GetDlgItem(window, IDC_DISASM_NEXT), FALSE);
        EnableWindow(GetDlgItem(window, IDC_DISASM_PREV), FALSE);
        return;
    }
    if (page >= limit) page = historyCount = 0;
    cursor = page;
    text[0] = 0;
    for (line = 0; line < 256 && cursor < limit; ++line)
    {
        start = cursor;
        lineAddresses[lineCount++] = displayBase + start;
        truncated = FALSE;
        if (z80)
        {
            /* The legacy decoder has no bounds argument. Pad a local instruction
               so a prefix or operand at the end of RAM cannot overread it. */
            unsigned char data[8] = {0};
            char decoded[128];
            int consumed = 0;
            for (i = 0; i < sizeof(data) && start + i < limit; ++i)
                data[i] = RomByte(start + i);
            if (!z80dis(data, &consumed, decoded) || consumed <= 0 ||
                (unsigned)consumed > limit - start)
            {
                cursor = start + 1;
                sprintf(instruction, "DC.B $%02X ; truncated instruction", data[0]);
            }
            else
            {
                cursor += consumed;
                strcpy(instruction, decoded + 18);
                instruction[strcspn(instruction, "\r\n")] = 0;
            }
        }
        else if (limit - cursor == 1)
        {
            sprintf(instruction, "DC.B $%02X", RomByte(cursor));
            ++cursor;
        }
        else if (sh2)
        {
            unsigned short opcode = NextWord();
            SH2Disasm(instruction, displayBase + start, opcode, 0);
        }
        else
        {
            strcpy(instruction, M68KDisasm(NextWord, NextLong));
            if (truncated)
            {
                cursor = min(start + 2, limit);
                sprintf(instruction, "DC.W $%04X ; truncated instruction", (RomByte(start) << 8) | RomByte(start + 1));
            }
        }
        count = min(cursor - start, 16);
        bytes[0] = 0;
        for (i = 0; i < count; ++i) sprintf(bytes + i * 3, "%02X ", RomByte(start + i));
        used += sprintf(text + used, "%08X  %-48s %s\r\n", start + displayBase, bytes, instruction);
    }
    nextPage = min(cursor, limit);
    SetDisassemblyText(text);
    HighlightPC();
    sprintf(address, "%08X", page + displayBase);
    SetDlgItemText(window, IDC_DISASM_ADDRESS, address);
    sprintf(status, "%s %s / %u bytes / linear decoding (memory also contains data)",
        z80 ? "Z80" : sub68k ? "Sega CD 68000" : sh2 ? (slave ? "Slave SH2" : "Master SH2") : "Genesis 68000",
        pcData ? (sub68k ? "Program RAM" : z80 ? "RAM" : "PC memory") : (!sh2 && SegaCD_Started ? "Mega CD BIOS" : "ROM"), limit);
    SetDlgItemText(window, IDC_DISASM_STATUS, status);
    EnableWindow(GetDlgItem(window, IDC_DISASM_NEXT), nextPage < limit);
    EnableWindow(GetDlgItem(window, IDC_DISASM_PREV), historyCount != 0);
}

static void JumpToPC(void)
{
    unsigned pc, mapped;
    unsigned char *data = NULL;
    unsigned size = 0, base = 0, swap = 0;
    char status[128];
    if (!CPUActive())
    {
        SetDlgItemText(window, IDC_DISASM_STATUS, "No active CPU.");
        return;
    }
    if (z80)
    {
        pc = CurrentPC();
        /* Z80 RAM is mirrored at 0x2000-0x3fff. Do not read I/O handlers. */
        if (pc < 0x4000)
        {
            data = Ram_Z80; size = sizeof(Ram_Z80); base = pc & 0x2000;
        }
    }
    else if (sh2)
    {
        SH2_CONTEXT *cpu = slave ? &S_SH2 : &M_SH2;
        if (!_32X_Started)
        {
            SetDlgItemText(window, IDC_DISASM_STATUS, "The selected SH2 CPU is inactive.");
            return;
        }
        pc = SH2_Get_PC(cpu);
        mapped = pc & 0xDFFFFFFF; /* Cached and uncached aliases share backing memory. */
        if (mapped >= 0x02000000 && mapped < 0x02400000)
        {
            data = _32X_Rom; size = min(Rom_Size, sizeof(_32X_Rom));
            base = (pc & 0x20000000) | 0x02000000;
        }
        else if (mapped >= 0x06000000 && mapped < 0x06040000)
        {
            data = _32X_Ram; size = sizeof(_32X_Ram);
            base = (pc & 0x20000000) | 0x06000000;
        }
        else if (mapped < (slave ? sizeof(_32X_SSH2_Rom) : sizeof(_32X_MSH2_Rom)))
        {
            data = slave ? _32X_SSH2_Rom : _32X_MSH2_Rom;
            size = slave ? sizeof(_32X_SSH2_Rom) : sizeof(_32X_MSH2_Rom);
            base = pc & 0x20000000;
        }
        else if (pc >= 0xC0000000 && pc < 0xC0001000)
        {
            data = cpu->Cache; size = sizeof(cpu->Cache); base = 0xC0000000;
        }
    }
    else
    {
        struct STARSCREAM_PROGRAMREGION *region = sub68k ? sub68k_context.fetch : main68k_context.fetch;
        pc = CurrentPC();
        /* Use the CPU's actual fetch mapping, including Mega CD BIOS/RAM and 32X banks/BIOS. */
        if (region)
            for (; region->lowaddr != (unsigned)-1; ++region)
                if (pc >= region->lowaddr && pc <= region->highaddr)
                {
                    base = region->lowaddr;
                    size = region->highaddr - base + 1;
                    data = (unsigned char *)(region->offset + base);
                    swap = 1;
                    break;
                }
    }
    if (!data || pc - base >= size || (!z80 && (pc & 1)))
    {
        sprintf(status, "PC %08X is outside readable instruction memory.", pc);
        SetDlgItemText(window, IDC_DISASM_STATUS, status);
        return;
    }
    pcData = data; pcSize = size; pcSwap = swap; displayBase = base;
    page = pc - base;
    historyCount = 0;
    Render();
    SendDlgItemMessage(window, IDC_DISASM_TEXT, EM_SETSEL, 0, 0);
    SendDlgItemMessage(window, IDC_DISASM_TEXT, EM_SCROLLCARET, 0, 0);
}

void disassembly_clear(void)
{
    page = nextPage = historyCount = 0;
    lineCount = 0;
    UseROM();
    if (window)
    {
        SetDisassemblyText(sub68k ? "The Sega CD 68000 CPU is inactive." : "No cartridge ROM loaded.");
        SetDlgItemText(window, IDC_DISASM_STATUS, "Disassembly - read only");
        EnableWindow(GetDlgItem(window, IDC_DISASM_NEXT), FALSE);
        EnableWindow(GetDlgItem(window, IDC_DISASM_PREV), FALSE);
    }
}

void disassembly_reset(void)
{
    page = historyCount = 0;
    UseROM();
    if (window && IsWindowVisible(window)) Render();
}

static INT_PTR CALLBACK DisassemblyProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        window = hwnd;
        HandleWindow_KMod[DMODE_DISASSEMBLY - 1] = hwnd;
        font = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH, "Consolas");
        SendDlgItemMessage(hwnd, IDC_DISASM_TEXT, WM_SETFONT, (WPARAM)font, TRUE);
        SendDlgItemMessage(hwnd, IDC_DISASM_TEXT, EM_SETLIMITTEXT, 65535, 0);
        SendDlgItemMessage(hwnd, IDC_DISASM_TEXT, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
        SetTimer(hwnd, 1, 100, NULL);
        SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"Genesis 68000");
        SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"Z80");
        SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"Sega CD 68000");
        SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"32X SH2 Master");
        SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_ADDSTRING, 0, (LPARAM)"32X SH2 Slave");
        sh2 = slave = sub68k = z80 = FALSE;
        UseROM();
        SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_SETCURSEL, 0, 0);
        {
            RECT client;
            GetClientRect(hwnd, &client);
            SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.right, client.bottom));
        }
        Render();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd))
        {
            /* Init_SegaCD sets its started flag after disassembly_reset. */
            if (!lineCount && CPUActive()) Render();
            else HighlightPC();
        }
        return TRUE;
    case WM_SIZE:
        if (window)
        {
            int width = LOWORD(lParam), height = HIWORD(lParam);
            MoveWindow(GetDlgItem(hwnd, IDC_DISASM_TEXT), 12, 44, max(1, width - 24), max(1, height - 74), TRUE);
            MoveWindow(GetDlgItem(hwnd, IDC_DISASM_STATUS), 12, max(44, height - 24), max(1, width - 24), 20, TRUE);
        }
        return TRUE;
    case WM_GETMINMAXINFO:
        {
            RECT minimum = {0, 0, 536, 120};
            MapDialogRect(hwnd, &minimum);
            AdjustWindowRectEx(&minimum, GetWindowLong(hwnd, GWL_STYLE), FALSE,
                GetWindowLong(hwnd, GWL_EXSTYLE));
            ((MINMAXINFO *)lParam)->ptMinTrackSize.x = minimum.right - minimum.left;
            ((MINMAXINFO *)lParam)->ptMinTrackSize.y = minimum.bottom - minimum.top;
        }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_DISASM_CPU:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                int selected = (int)SendDlgItemMessage(hwnd, IDC_DISASM_CPU, CB_GETCURSEL, 0, 0);
                sh2 = selected == 3 || selected == 4;
                sub68k = selected == 2;
                z80 = selected == 1;
                slave = selected == 4;
                disassembly_reset();
            }
            return TRUE;
        case IDC_DISASM_PC:
            JumpToPC();
            return TRUE;
        case IDC_DISASM_ROM:
            disassembly_reset();
            Render();
            return TRUE;
        case IDC_DISASM_NEXT:
            if (nextPage < limit)
            {
                if (historyCount == 256)
                {
                    memmove(history, history + 1, 255 * sizeof(history[0]));
                    --historyCount;
                }
                history[historyCount++] = page;
                page = nextPage;
                Render();
            }
            return TRUE;
        case IDC_DISASM_PREV:
            if (historyCount) { page = history[--historyCount]; Render(); }
            return TRUE;
        case IDC_DISASM_GOTO:
            {
                char input[64], *end;
                unsigned long value;
                GetDlgItemText(hwnd, IDC_DISASM_ADDRESS, input, sizeof(input));
                errno = 0;
                value = strtoul(input, &end, 16);
                if (value >= displayBase && value - displayBase < limit) value -= displayBase;
                if (!input[0] || input[0] == '-' || errno == ERANGE || *end || value >= limit || (!z80 && (value & 1)))
                {
                    SetDlgItemText(hwnd, IDC_DISASM_STATUS, z80 ? "Enter a hexadecimal address within the displayed memory." : "Enter an even hexadecimal address within the displayed memory.");
                    return TRUE;
                }
                page = value;
                historyCount = 0;
                Render();
            }
            return TRUE;
        case IDCANCEL:
            CloseWindow_KMod(DMODE_DISASSEMBLY);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        CloseWindow_KMod(DMODE_DISASSEMBLY);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        DeleteObject(font);
        font = NULL;
        window = NULL;
        HandleWindow_KMod[DMODE_DISASSEMBLY - 1] = NULL;
        OpenedWindow_KMod[DMODE_DISASSEMBLY - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void disassembly_create(HINSTANCE instance, HWND parent)
{
    if (!richEdit) richEdit = LoadLibraryA("Riched20.dll");
    if (!richEdit) return;
    CreateDialog(instance, MAKEINTRESOURCE(IDD_DISASSEMBLY), parent, DisassemblyProc);
}

void disassembly_show(BOOL visible)
{
    if (visible) Render();
    ShowWindow(window, visible ? SW_SHOW : SW_HIDE);
}

void disassembly_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "DisassemblyOpen",
        OpenedWindow_KMod[DMODE_DISASSEMBLY - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(window, "DisassemblyRect", config);
}

void disassembly_restore_window(const char *config)
{
    BOOL visible = GetPrivateProfileInt("DebugWindows", "DisassemblyOpen", 0, config) != 0;
    DebugWindow_RestoreGeometry(window, "DisassemblyRect", config);
    OpenedWindow_KMod[DMODE_DISASSEMBLY - 1] = visible && window != NULL;
    disassembly_show(visible);
}
