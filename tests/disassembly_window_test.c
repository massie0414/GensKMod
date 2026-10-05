#include <assert.h>
#include "../src/Gens/kmod/disassembly.c"

struct Rom *Game;
int Genesis_Started, _32X_Started;
struct S68000CONTEXT main68k_context;
SH2_CONTEXT M_SH2, S_SH2;
unsigned char _32X_Rom[4 * 1024 * 1024], _32X_Ram[256 * 1024];
unsigned char _32X_MSH2_Rom[2048], _32X_SSH2_Rom[1024];
UINT32 FASTCALL SH2_Get_PC(SH2_CONTEXT *cpu) { return cpu->PC; }
unsigned char Rom_Data[8 * 1024 * 1024];
unsigned int Rom_Size;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    disassembly_show(FALSE);
}

static void Go(const char *address)
{
    SetDlgItemText(window, IDC_DISASM_ADDRESS, address);
    SendMessage(window, WM_COMMAND, IDC_DISASM_GOTO, 0);
}

static COLORREF BackgroundAt(int line)
{
    HWND text = GetDlgItem(window, IDC_DISASM_TEXT);
    CHARFORMAT2A format = {0};
    CHARRANGE range;
    range.cpMin = (LONG)SendMessage(text, EM_LINEINDEX, line, 0);
    range.cpMax = range.cpMin + 1;
    SendMessage(text, EM_EXSETSEL, 0, (LPARAM)&range);
    format.cbSize = sizeof(format);
    SendMessage(text, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    assert(format.dwMask & CFM_BACKCOLOR);
    return format.crBackColor;
}

int main(void)
{
    char text[65536];
    unsigned char ram[16] = {0x71, 0x4e, 0x75, 0x4e};
    struct STARSCREAM_PROGRAMREGION fetch[3];
    unsigned i;
    RECT before, after;
    disassembly_create(GetModuleHandle(NULL), NULL);
    assert(window);
    disassembly_show(TRUE);
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(strstr(text, "No cartridge ROM"));
    assert(GetWindowLong(GetDlgItem(window, IDC_DISASM_TEXT), GWL_STYLE) & ES_READONLY);

    Game = (struct Rom *)1;
    Genesis_Started = 1;
    Rom_Size = 1024;
    /* 68000 NOP followed by MOVE.L #$12345678,D0, then NOPs. */
    for (i = 0; i < Rom_Size; i += 2) { Rom_Data[i] = 0x71; Rom_Data[i + 1] = 0x4e; }
    Rom_Data[2] = 0x3c; Rom_Data[3] = 0x20;
    Rom_Data[4] = 0x34; Rom_Data[5] = 0x12;
    Rom_Data[6] = 0x78; Rom_Data[7] = 0x56;
    disassembly_reset();
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(strstr(text, "NOP") && strstr(text, "12345678"));
    assert(strstr(text, "00000008") && !strstr(text, "00000004  "));
    assert(nextPage == 516);
    fetch[0].lowaddr = 0; fetch[0].highaddr = Rom_Size - 1;
    fetch[0].offset = (unsigned)Rom_Data;
    fetch[1].lowaddr = 0xff0000; fetch[1].highaddr = 0xff000f;
    fetch[1].offset = (unsigned)ram - 0xff0000;
    fetch[2].lowaddr = (unsigned)-1;
    main68k_context.fetch = fetch;
    main68k_context.pc = 8;
    Render();
    assert(highlightedLine == 2 && BackgroundAt(2) == RGB(255, 255, 128));
    assert(BackgroundAt(1) == RGB(255, 255, 255));
    {
        CHARRANGE selection = {3, 9}, afterSelection;
        SendDlgItemMessage(window, IDC_DISASM_TEXT, EM_EXSETSEL, 0, (LPARAM)&selection);
        main68k_context.pc = 2;
        SendMessage(window, WM_TIMER, 1, 0);
        SendDlgItemMessage(window, IDC_DISASM_TEXT, EM_EXGETSEL, 0, (LPARAM)&afterSelection);
        assert(afterSelection.cpMin == 3 && afterSelection.cpMax == 9);
        assert(highlightedLine == 1 && BackgroundAt(1) == RGB(255, 255, 128));
        assert(BackgroundAt(2) == RGB(255, 255, 255));
        main68k_context.pc = 8;
    }
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    assert(page == 8 && pcData == Rom_Data);
    assert(highlightedLine == 0 && BackgroundAt(0) == RGB(255, 255, 128));
    main68k_context.pc = 0xff0002;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(page == 2 && strstr(text, "00FF0002") && strstr(text, "RTS"));
    assert(main68k_context.pc == 0xff0002 && ram[2] == 0x75);
    main68k_context.pc = 0xa00000;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    assert(page == 2 && pcData == ram);
    SendMessage(window, WM_TIMER, 1, 0);
    assert(highlightedLine == -1 && BackgroundAt(0) == RGB(255, 255, 255));
    SendMessage(window, WM_COMMAND, IDC_DISASM_ROM, 0);
    assert(page == 0 && !pcData);
    SendMessage(window, WM_COMMAND, IDC_DISASM_NEXT, 0);
    assert(page == 516 && historyCount == 1);
    SendMessage(window, WM_COMMAND, IDC_DISASM_PREV, 0);
    assert(page == 0 && historyCount == 0);
    Go("3FE"); assert(page == 1022 && nextPage == 1024);
    Go("3FF"); assert(page == 1022);
    Go("100000000"); assert(page == 1022);
    Go("-2"); assert(page == 1022);
    Go("GG"); assert(page == 1022);

    Rom_Size = 2; Rom_Data[0] = 0x3c; Rom_Data[1] = 0x20;
    disassembly_reset();
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(strstr(text, "truncated instruction") && nextPage == 2);
    /* The trailing odd byte is stored at its swapped physical address. */
    Rom_Size = 3; Rom_Data[0] = 0x71; Rom_Data[1] = 0x4e; Rom_Data[3] = 0xab;
    disassembly_reset();
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(strstr(text, "DC.B $AB"));

    _32X_Started = 1;
    Rom_Size = 4; Rom_Data[0] = 0x09; Rom_Data[1] = 0x00; Rom_Data[2] = 0x0b; Rom_Data[3] = 0x00;
    SendDlgItemMessage(window, IDC_DISASM_CPU, CB_SETCURSEL, 1, 0);
    SendMessage(window, WM_COMMAND, MAKEWPARAM(IDC_DISASM_CPU, CBN_SELCHANGE), 0);
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(strstr(text, "02000000") && strstr(text, "nop") && strstr(text, "rts"));
    Go("02000002"); assert(page == 2);
    memcpy(_32X_Rom, "\x00\x09\x00\x0b", 4);
    M_SH2.PC = 0x22000002;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(page == 2 && strstr(text, "22000002") && strstr(text, "rts"));
    assert(highlightedLine == 0 && BackgroundAt(0) == RGB(255, 255, 128));
    SendMessage(window, WM_COMMAND, IDC_DISASM_ROM, 0);
    assert(highlightedLine == 1 && BackgroundAt(1) == RGB(255, 255, 128));
    _32X_Ram[0] = 0; _32X_Ram[1] = 9;
    M_SH2.PC = 0x26000000;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    assert(page == 0 && pcData == _32X_Ram && displayBase == 0x26000000);
    SendDlgItemMessage(window, IDC_DISASM_CPU, CB_SETCURSEL, 2, 0);
    SendMessage(window, WM_COMMAND, MAKEWPARAM(IDC_DISASM_CPU, CBN_SELCHANGE), 0);
    S_SH2.PC = 0x00000002;
    _32X_SSH2_Rom[2] = 0; _32X_SSH2_Rom[3] = 9;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    assert(slave && page == 2 && pcData == _32X_SSH2_Rom);
    assert(highlightedLine == 0 && BackgroundAt(0) == RGB(255, 255, 128));
    S_SH2.PC = 0xc0000002;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    assert(pcData == S_SH2.Cache && displayBase == 0xc0000000);
    _32X_Started = 0;
    SendMessage(window, WM_COMMAND, IDC_DISASM_PC, 0);
    assert(pcData == S_SH2.Cache);
    _32X_Started = 1;
    SendMessage(window, WM_COMMAND, IDC_DISASM_ROM, 0);
    assert(Rom_Data[0] == 0x09 && Rom_Data[1] == 0x00);

    GetWindowRect(GetDlgItem(window, IDC_DISASM_TEXT), &before);
    GetWindowRect(window, &after);
    SetWindowPos(window, NULL, 0, 0, after.right - after.left + 100,
        after.bottom - after.top + 100, SWP_NOMOVE | SWP_NOZORDER);
    GetWindowRect(GetDlgItem(window, IDC_DISASM_TEXT), &after);
    assert(after.right - after.left == before.right - before.left + 100);
    assert(after.bottom - after.top == before.bottom - before.top + 100);
    disassembly_clear();
    GetDlgItemText(window, IDC_DISASM_TEXT, text, sizeof(text));
    assert(strstr(text, "No cartridge ROM"));
    assert(highlightedLine == -1 && BackgroundAt(0) == RGB(255, 255, 255));
    disassembly_reset();
    assert(page == 0 && nextPage == 4);
    SendMessage(window, WM_COMMAND, IDCANCEL, 0);
    assert(!IsWindowVisible(window));
    DestroyWindow(window);
    assert(!HandleWindow_KMod[DMODE_DISASSEMBLY - 1]);
    puts("PASS: yellow PC background, highlight movement/removal, preserved selection, SH2 aliases, real decoding, PC jumps, ROM return, paging, bounds, read-only, resize and reset");
    return 0;
}
