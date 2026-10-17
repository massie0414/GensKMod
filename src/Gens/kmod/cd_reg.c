#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../Mem_M68k.h"
#include "../Mem_S68k.h"

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "cd_reg.h"
#include "window_geometry.h"

static HWND hCD_Reg, hCDRegList;

static void CDRegFitList_KMod(HWND hwnd)
{
	RECT listRect, clientRect, rowRect, dialogRect;
	int width, height, column;
	GetWindowRect(hCDRegList, &listRect);
	GetClientRect(hCDRegList, &clientRect);
	if (!ListView_GetItemRect(hCDRegList, 0, &rowRect, LVIR_BOUNDS)) return;
	width = (listRect.right - listRect.left) - clientRect.right;
	for (column = 0; column < 3; column++)
		width += ListView_GetColumnWidth(hCDRegList, column);
	height = rowRect.top +
		(rowRect.bottom - rowRect.top) * ListView_GetItemCount(hCDRegList) +
		(listRect.bottom - listRect.top) - clientRect.bottom;
	GetWindowRect(hwnd, &dialogRect);
	SetWindowPos(hCDRegList, NULL, 0, 0, width, height,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(hwnd, NULL, 0, 0,
		dialogRect.right - dialogRect.left + width - (listRect.right - listRect.left),
		dialogRect.bottom - dialogRect.top + height - (listRect.bottom - listRect.top),
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CDRegInit_KMod(HWND hwnd)
{
	LV_COLUMN   lvColumn;
	LVITEM		lvItem;
	int         i;
	char		buf[64];
	TCHAR       szString[3][20] = { "Address (hex)", "MainCPU (0xA12000)", "SubCPU (0xFF8000)" };

	hCDRegList = GetDlgItem(hwnd, IDC_CD_REG_LIST);
	ListView_DeleteAllItems(hCDRegList);

	while (ListView_DeleteColumn(hCDRegList, 0)) {}

	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT /*| LVCF_SUBITEM*/;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = ListView_GetStringWidth(hCDRegList, szString[0]) +
		ListView_GetStringWidth(hCDRegList, "    ");
	lvColumn.pszText = szString[0];
	ListView_InsertColumn(hCDRegList, 0, &lvColumn);

	lvColumn.cx = ListView_GetStringWidth(hCDRegList, szString[1]) +
		ListView_GetStringWidth(hCDRegList, "    ");
	lvColumn.pszText = szString[1];
	ListView_InsertColumn(hCDRegList, 1, &lvColumn);

	lvColumn.cx = ListView_GetStringWidth(hCDRegList, szString[2]) +
		ListView_GetStringWidth(hCDRegList, "    ");
	lvColumn.pszText = szString[2];
	ListView_InsertColumn(hCDRegList, 2, &lvColumn);

	ListView_SetExtendedListViewStyle(hCDRegList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	for (i = 0; i <= 0x4A; i += 2)
	{
		lvItem.mask = LVIF_TEXT;
		lvItem.iSubItem = 0;
		wsprintf(buf, "%0.2X", i);
		lvItem.pszText = buf;
		lvItem.iItem = i / 2;
		ListView_InsertItem(hCDRegList, &lvItem);

		lvItem.iSubItem = 1;
		if (i > 0x2E)
			wsprintf(buf, "");
		else
			wsprintf(buf, "0x%0.4X", M68K_RW(0xA12000 | i));
		lvItem.pszText = buf;
		ListView_SetItem(hCDRegList, &lvItem);

		lvItem.iSubItem = 2;
		wsprintf(buf, "0x%0.4X", S68K_RW(0xFF8000 | i));
		lvItem.pszText = buf;
		ListView_SetItem(hCDRegList, &lvItem);
	}
}

void UpdateCD_Reg_KMod()
{
	int         i;
	char		buf[64];
	LVITEM		lvItem;

	for (i = 0; i <= 0x4A; i += 2)
	{
		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i / 2;

		lvItem.iSubItem = 1;
		if (i > 0x2E)
			wsprintf(buf, "");
		else
			wsprintf(buf, "0x%0.4X", M68K_RW(0xA12000 | i));
		lvItem.pszText = buf;
		ListView_SetItem(hCDRegList, &lvItem);

		lvItem.iSubItem = 2;
		wsprintf(buf, "0x%0.4X", S68K_RW(0xFF8000 | i));
		lvItem.pszText = buf;
		ListView_SetItem(hCDRegList, &lvItem);
	}
}


BOOL CALLBACK CD_RegDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	switch (Message)
	{
	case WM_INITDIALOG:
		HandleWindow_KMod[DMODE_CD_REG - 1] = hwnd;
		CDRegInit_KMod(hwnd);
		CDRegFitList_KMod(hwnd);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) != IDCANCEL) return FALSE;
	case WM_CLOSE:
		CloseWindow_KMod(DMODE_CD_REG);
		break;

	case WM_DESTROY:
		hCD_Reg = hCDRegList = NULL;
		HandleWindow_KMod[DMODE_CD_REG - 1] = NULL;
		OpenedWindow_KMod[DMODE_CD_REG - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


void cdreg_create(HINSTANCE hInstance, HWND hWndParent)
{
	hCD_Reg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGCD_REG), hWndParent, CD_RegDlgProc);
}

void cdreg_show(BOOL visibility)
{
	ShowWindow(hCD_Reg, visibility ? SW_SHOW : SW_HIDE);
}

void cdreg_update()
{
	if (OpenedWindow_KMod[DMODE_CD_REG-1] == FALSE)	return;

	UpdateCD_Reg_KMod();
}

void cdreg_reset()
{
	CDRegInit_KMod(hCD_Reg);
}
void cdreg_destroy()
{
	if (hCD_Reg) DestroyWindow(hCD_Reg);
}

void cdreg_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "CDRegOpen",
		OpenedWindow_KMod[DMODE_CD_REG - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(hCD_Reg, "CDRegRect", config_file);
}

void cdreg_restore_window(const char *config_file)
{
	RECT rect;
	BOOL visible = GetPrivateProfileInt("DebugWindows", "CDRegOpen", 0, config_file) != 0;
	if (!hCD_Reg) return;
	GetWindowRect(hCD_Reg, &rect);
	DebugWindow_RestoreGeometry(hCD_Reg, "CDRegRect", config_file);
	/* Keep the size measured for all registers at the current font size. */
	SetWindowPos(hCD_Reg, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	OpenedWindow_KMod[DMODE_CD_REG - 1] = visible;
	cdreg_show(visible);
}

