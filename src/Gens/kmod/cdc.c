#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../LC89510.h"

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "cdc.h"
#include "window_geometry.h"

static HWND hCD_CDC;
HWND hCDCDCList;

static void CD_CDCFitList(HWND hwnd)
{
	RECT listRect, clientRect, rowRect, dialogRect;
	int width, height, column;

	/* Use actual font metrics so all registers fit at the current DPI. */
	for (column = 0; column < 2; column++)
		ListView_SetColumnWidth(hCDCDCList, column, LVSCW_AUTOSIZE);
	GetWindowRect(hCDCDCList, &listRect);
	GetClientRect(hCDCDCList, &clientRect);
	if (!ListView_GetItemRect(hCDCDCList, 0, &rowRect, LVIR_BOUNDS)) return;
	width = ListView_GetColumnWidth(hCDCDCList, 0) +
		ListView_GetColumnWidth(hCDCDCList, 1) +
		(listRect.right - listRect.left) - clientRect.right;
	height = rowRect.top +
		(rowRect.bottom - rowRect.top) * ListView_GetItemCount(hCDCDCList) +
		(listRect.bottom - listRect.top) - clientRect.bottom;
	GetWindowRect(hwnd, &dialogRect);
	SetWindowPos(hCDCDCList, NULL, 0, 0, width, height,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	SetWindowPos(hwnd, NULL, 0, 0,
		dialogRect.right - dialogRect.left + width - (listRect.right - listRect.left),
		dialogRect.bottom - dialogRect.top + height - (listRect.bottom - listRect.top),
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CD_CDCInit_KMod(HWND hwnd)
{
	LV_COLUMN   lvColumn;
	LVITEM		lvItem;
	int column;
	//	int         i;
	char		buf[64];
	TCHAR       szString[2][20] = { "Name", "Value" };

	hCDCDCList = GetDlgItem(hwnd, IDC_CDC_LIST);
	ListView_DeleteAllItems(hCDCDCList);

	/* Reset reinitializes this list too; avoid accumulating duplicate columns. */
	while (ListView_DeleteColumn(hCDCDCList, 0)) {}

	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT /*| LVCF_SUBITEM*/;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 80;
	lvColumn.pszText = szString[0];
	ListView_InsertColumn(hCDCDCList, 0, &lvColumn);

	lvColumn.cx = 80;
	lvColumn.pszText = szString[1];
	ListView_InsertColumn(hCDCDCList, 1, &lvColumn);


	ListView_SetExtendedListViewStyle(hCDCDCList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	lvItem.mask = LVIF_TEXT;

	lvItem.iItem = 0;
	lvItem.iSubItem = 0;
	lvItem.pszText = "COMIN";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.2X", CDC.COMIN);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 1;
	lvItem.iSubItem = 0;
	lvItem.pszText = "IFSTAT";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.2X", CDC.IFSTAT);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 2;
	lvItem.iSubItem = 0;
	lvItem.pszText = "DBC";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.DBC);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 3;
	lvItem.iSubItem = 0;
	lvItem.pszText = "HEAD";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.8X", CDC.HEAD);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 4;
	lvItem.iSubItem = 0;
	lvItem.pszText = "PT";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.PT);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 5;
	lvItem.iSubItem = 0;
	lvItem.pszText = "WA";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.WA);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 6;
	lvItem.iSubItem = 0;
	lvItem.pszText = "STAT";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.8X", CDC.STAT);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 7;
	lvItem.iSubItem = 0;
	lvItem.pszText = "CTRL";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.8X", CDC.CTRL);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 8;
	lvItem.iSubItem = 0;
	lvItem.pszText = "DAC";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.DAC);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 9;
	lvItem.iSubItem = 0;
	lvItem.pszText = "IFCTRL";
	ListView_InsertItem(hCDCDCList, &lvItem);
	//	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.2X", CDC.IFCTRL);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);
	for (column = 0; column < 2; column++)
		ListView_SetColumnWidth(hCDCDCList, column, LVSCW_AUTOSIZE);
}

void UpdateCD_CDC_KMod()
{
	//	int         i;
	char		buf[64];
	LVITEM		lvItem;
	
	lvItem.mask = LVIF_TEXT;
	lvItem.iSubItem = 1;


	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 0;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.2X", CDC.COMIN);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 1;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.2X", CDC.IFSTAT);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 2;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.DBC);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 3;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.8X", CDC.HEAD);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 4;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.PT);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 5;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.WA);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 6;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.8X", CDC.STAT);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 7;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.8X", CDC.CTRL);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 8;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.4X", CDC.DAC);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);

	//	lvItem.mask = LVIF_TEXT;
	lvItem.iItem = 9;
	//	lvItem.iSubItem = 1;
	wsprintf(buf, "0x%0.2X", CDC.IFCTRL);
	lvItem.pszText = buf;
	ListView_SetItem(hCDCDCList, &lvItem);
}


BOOL CALLBACK CD_CDCDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	switch (Message)
	{
	case WM_INITDIALOG:
		HandleWindow_KMod[DMODE_CD_CDC - 1] = hwnd;
		CD_CDCInit_KMod(hwnd);
		CD_CDCFitList(hwnd);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) != IDCANCEL) return FALSE;
	case WM_CLOSE:
		CloseWindow_KMod(DMODE_CD_CDC);
		break;

	case WM_DESTROY:
		hCD_CDC = NULL;
		hCDCDCList = NULL;
		HandleWindow_KMod[DMODE_CD_CDC - 1] = NULL;
		OpenedWindow_KMod[DMODE_CD_CDC - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

void cdcdebug_create(HINSTANCE hInstance, HWND hWndParent)
{
	hCD_CDC = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGCD_CDC), hWndParent, CD_CDCDlgProc);
}

void cdcdebug_show(BOOL visibility)
{
	ShowWindow(hCD_CDC, visibility ? SW_SHOW : SW_HIDE);
}

void cdcdebug_update()
{
	if (OpenedWindow_KMod[DMODE_CD_CDC-1] == FALSE)	return;

	UpdateCD_CDC_KMod();
}

void cdcdebug_reset()
{
	//TODO remove and create a real reset (all to 0) ?
	CD_CDCInit_KMod(hCD_CDC);
}
void cdcdebug_destroy()
{
	if (hCD_CDC) DestroyWindow(hCD_CDC);
}

void cdcdebug_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "CDCOpen",
		OpenedWindow_KMod[DMODE_CD_CDC - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(hCD_CDC, "CDCRect", config_file);
}

void cdcdebug_restore_window(const char *config_file)
{
	RECT rect;
	BOOL visible = GetPrivateProfileInt("DebugWindows", "CDCOpen", 0, config_file) != 0;
	if (!hCD_CDC) return;
	GetWindowRect(hCD_CDC, &rect);
	DebugWindow_RestoreGeometry(hCD_CDC, "CDCRect", config_file);
	/* Retain the fixed size calculated for the current font and all ten rows. */
	SetWindowPos(hCD_CDC, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	OpenedWindow_KMod[DMODE_CD_CDC - 1] = visible;
	cdcdebug_show(visible);
}

