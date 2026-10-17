#include <assert.h>
#include "../src/Gens/kmod/cd_reg.c"

UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
unsigned short M68K_RW(unsigned int address) { return (unsigned short)address; }
unsigned short S68K_RW(unsigned int address) { return (unsigned short)address; }
void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    cdreg_show(FALSE);
}

static void CheckList(void)
{
    RECT client, row;
    char text[32];
    int column, width = 0;
    assert(ListView_GetItemCount(hCDRegList) == 38);
    assert(Header_GetItemCount(ListView_GetHeader(hCDRegList)) == 3);
    assert(!(GetWindowLong(hCDRegList, GWL_STYLE) & (WS_HSCROLL | WS_VSCROLL)));
    GetClientRect(hCDRegList, &client);
    assert(ListView_GetItemRect(hCDRegList, 37, &row, LVIR_BOUNDS));
    assert(row.bottom <= client.bottom);
    for (column = 0; column < 3; column++) width += ListView_GetColumnWidth(hCDRegList, column);
    assert(width <= client.right);
    ListView_GetItemText(hCDRegList, 37, 0, text, sizeof(text));
    assert(!strcmp(text, "4A"));
    ListView_GetItemText(hCDRegList, 37, 2, text, sizeof(text));
    assert(!strcmp(text, "0x804A"));
    ListView_GetItemText(hCDRegList, 23, 1, text, sizeof(text));
    assert(!strcmp(text, "0x202E"));
}

int main(void)
{
    INITCOMMONCONTROLSEX ic = {sizeof(ic), ICC_LISTVIEW_CLASSES};
    char folder[MAX_PATH], config[MAX_PATH];
    RECT saved, restored;
    MONITORINFO monitor = {sizeof(monitor)};
    InitCommonControlsEx(&ic);
    GetTempPath(sizeof(folder), folder);
    assert(GetTempFileName(folder, "cdr", 0, config));
    cdreg_create(GetModuleHandle(NULL), NULL);
    assert(hCD_Reg && HandleWindow_KMod[DMODE_CD_REG - 1] == hCD_Reg);
    cdreg_restore_window(config);
    assert(!IsWindowVisible(hCD_Reg));
    CheckList();
    cdreg_reset();
    CheckList();
    GetMonitorInfo(MonitorFromWindow(hCD_Reg, MONITOR_DEFAULTTONEAREST), &monitor);
    SetWindowPos(hCD_Reg, NULL, monitor.rcWork.left + 40, monitor.rcWork.top + 40,
        0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_CD_REG - 1] = TRUE;
    cdreg_show(TRUE);
    GetWindowRect(hCD_Reg, &saved);
    cdreg_save_window(config);
    cdreg_destroy();
    assert(!hCD_Reg && !HandleWindow_KMod[DMODE_CD_REG - 1]);
    cdreg_create(GetModuleHandle(NULL), NULL);
    cdreg_restore_window(config);
    assert(IsWindowVisible(hCD_Reg) && OpenedWindow_KMod[DMODE_CD_REG - 1]);
    GetWindowRect(hCD_Reg, &restored);
    assert(EqualRect(&saved, &restored));
    CheckList();
    assert(!(GetWindowLong(hCD_Reg, GWL_STYLE) & (WS_THICKFRAME | WS_MAXIMIZEBOX)));
    SendMessage(hCD_Reg, WM_COMMAND, IDCANCEL, 0);
    assert(!IsWindowVisible(hCD_Reg) && !OpenedWindow_KMod[DMODE_CD_REG - 1]);
    cdreg_save_window(config);
    cdreg_destroy();
    cdreg_create(GetModuleHandle(NULL), NULL);
    cdreg_restore_window(config);
    assert(!IsWindowVisible(hCD_Reg) && !OpenedWindow_KMod[DMODE_CD_REG - 1]);
    cdreg_destroy();
    DeleteFile(config);
    puts("PASS: 38 rows and 3 columns fit; initial values, reset, position and visibility restoration");
    return 0;
}
