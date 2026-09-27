#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../Mem_SH2.h"
#include "../mem_M68K.h"
#include "../pwm.h"
#include "../vdp_32X.h"

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "s32x_reg.h"
#include "window_geometry.h"

static HWND h32X_Reg;
static HWND h32XRegList;
static SIZE minimum32XRegSize;

static void Resize32XRegList(HWND hwnd)
{
	RECT client, margin = {0, 0, 7, 7};
	if (!h32XRegList) return;
	GetClientRect(hwnd, &client);
	MapDialogRect(hwnd, &margin);
	MoveWindow(h32XRegList, margin.right, margin.bottom,
		max(0, client.right - 2 * margin.right),
		max(0, client.bottom - 2 * margin.bottom), TRUE);
}

/* Inspect backing state, never CPU bus handlers: those consume FIFO data,
 * charge SH2 cycles, and toggle the VDP status register on reads.
 * The 32X column is the master SH2 view. Return FALSE for write-only ports.
 */
static BOOL Peek32XRegister(WORD address, BOOL sh2, WORD *value)
{
	unsigned int result;
	if (address >= 0x20 && address <= 0x2E)
	{
		*value = (_32X_Comm[address - 0x20] << 8) | _32X_Comm[address - 0x20 + 1];
		return TRUE;
	}
	switch (address)
	{
	case 0x00:
		result = sh2 ? (((_32X_FM | (_32X_ADEN << 1) | (CD_32X_Active != 0)) << 8) | _32X_MINT)
			: ((_32X_FM << 8) | _32X_ADEN | _32X_RES | 0x80);
		break;
	case 0x04: result = sh2 ? _32X_HIC : (Bank_SH2 & 0xFF); break;
	case 0x06:
		result = sh2 ? (_32X_DREQ_ST | _32X_RV)
			: ((_32X_DREQ_ST & 0xFF) | ((_32X_DREQ_ST >> 8) & 0x80) | _32X_RV);
		break;
	case 0x08: result = _32X_DREQ_SRC >> 16; break;
	case 0x0A: result = _32X_DREQ_SRC; break;
	case 0x0C: result = _32X_DREQ_DST >> 16; break;
	case 0x0E: result = _32X_DREQ_DST; break;
	case 0x10: result = _32X_DREQ_LEN; break;
	case 0x12:
		if (!sh2) return FALSE;
		result = 0;
		if ((_32X_DREQ_ST & 0x4004) == 4 && _32X_FIFO_Read < 4)
			result = (_32X_FIFO_Block == 0 ? _32X_FIFO_B : _32X_FIFO_A)[_32X_FIFO_Read];
		break;
	case 0x30: result = PWM_Mode; break;
	case 0x32: result = PWM_Cycle_Tmp; break;
	case 0x34:
	case 0x38: result = PWM_FULL_TAB[(PWM_RP_L & 3) * 4 + (PWM_WP_L & 3)] << 8; break;
	case 0x36: result = PWM_FULL_TAB[(PWM_RP_R & 3) * 4 + (PWM_WP_R & 3)] << 8; break;
	case 0x100: result = _32X_VDP.Mode; break;
	case 0x102: result = (_32X_VDP.Mode >> 16) & 0xFF; break;
	case 0x104: result = _32X_VDP.AF_Len & 0xFF; break;
	case 0x106: result = _32X_VDP.AF_St; break;
	case 0x108: result = _32X_VDP.AF_Data; break;
	case 0x10A: result = _32X_VDP.State; break;
	default: return FALSE;
	}
	*value = (WORD)result;
	return TRUE;
}



struct _32X_register_struct
{
	BYTE side; //bit 0:MD, bit 1:32X
	WORD adr;
	char *descriptionMD;
	char *description32X;
};



const struct _32X_register_struct _32X_register[] =
{
	{ 3, 0x00, "Adapter control register", "Interrupt Mask" },
	{ 3, 0x02, "Interrupt control register", "Stand By change" },
	{ 3, 0x04, "Bank set register", "H Count" },
	{ 3, 0x06, "DREQ control register", "DREQ control register" },
	{ 3, 0x08, "DREQ source MSB", "DREQ source MSB" },
	{ 3, 0x0A, "DREQ source LSB", "DREQ source LSB" },
	{ 3, 0x0C, "DREQ destination MSB", "DREQ destination MSB" },
	{ 3, 0x0E, "DREQ destination LSB", "DREQ destination LSB" },
	{ 3, 0x10, "DREQ length", "DREQ length" },
	{ 3, 0x12, "FIFO", "FIFO" },
	{ 2, 0x14, "", "VRES Interrupt clear" },
	{ 2, 0x16, "", "V Interrupt clear" },
	{ 2, 0x18, "", "H Interrupt clear" },
	{ 3, 0x1A, "SEGA TV register", "CMD Interrupt clear" },
	{ 2, 0x1C, "", "PWM Interrupt clear" },
	{ 3, 0x20, "Comm Port 0", "Comm Port 0" },
	{ 3, 0x22, "Comm Port 1", "Comm Port 1" },
	{ 3, 0x24, "Comm Port 2", "Comm Port 2" },
	{ 3, 0x26, "Comm Port 3", "Comm Port 3" },
	{ 3, 0x28, "Comm Port 4", "Comm Port 4" },
	{ 3, 0x2A, "Comm Port 5", "Comm Port 5" },
	{ 3, 0x2C, "Comm Port 6", "Comm Port 6" },
	{ 3, 0x2E, "Comm Port 7", "Comm Port 7" },
	{ 3, 0x30, "PWM control", "PWM control" },
	{ 3, 0x32, "Cycle register", "Cycle register" },
	{ 3, 0x34, "L ch Pulse Width", "L ch Pulse Width" },
	{ 3, 0x36, "R ch Pulse Width", "R ch Pulse Width" },
	{ 3, 0x38, "Mono Pulse Width", "Mono Pulse Width" },
	{ 2, 0x100, "", "Bitmap mode" },
	{ 2, 0x102, "", "Packed Pixel Control" },
	{ 2, 0x104, "", "Auto fill length" },
	{ 2, 0x106, "", "Auto fill start" },
	{ 2, 0x108, "", "Auto fill data" },
	{ 2, 0x10A, "", "Frame Buffer control" },
	{ 0xff, -1, "" }
};




void _32X_RegInit_KMod(HWND hwnd)
{
	LV_COLUMN   lvColumn = {0};
	LVITEM		lvItem = {0};
	int         i, adr;
	char		buf[64];
	TCHAR       szString[6][20] = { "Description", "MD Address", "Value", "Value", "32X Address", "Description" };

	h32XRegList = GetDlgItem(hwnd, IDC_32XREG_LIST);
	if (!h32XRegList) return;
	ListView_DeleteAllItems(h32XRegList);
	while (ListView_DeleteColumn(h32XRegList, 0)) { }

	//	GetWindowRect( h32XRegList, &rSize);

	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT /*| LVCF_SUBITEM*/;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.cx = 140;
	lvColumn.pszText = szString[0];
	ListView_InsertColumn(h32XRegList, 0, &lvColumn);

	lvColumn.cx = 80;
	lvColumn.pszText = szString[1];
	ListView_InsertColumn(h32XRegList, 1, &lvColumn);

	lvColumn.cx = 60;
	lvColumn.pszText = szString[2];
	ListView_InsertColumn(h32XRegList, 2, &lvColumn);

	lvColumn.cx = 60;
	lvColumn.pszText = szString[3];
	ListView_InsertColumn(h32XRegList, 3, &lvColumn);

	lvColumn.cx = 80;
	lvColumn.pszText = szString[4];
	ListView_InsertColumn(h32XRegList, 4, &lvColumn);

	lvColumn.cx = 140;
	lvColumn.pszText = szString[5];
	ListView_InsertColumn(h32XRegList, 5, &lvColumn);

	ListView_SetExtendedListViewStyle(h32XRegList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);


	i = 0;
	while (_32X_register[i].side != 0xff)
	{

		lvItem.mask = LVIF_TEXT;
		lvItem.iItem = i;
		lvItem.iSubItem = 0;
		lvItem.pszText = _32X_register[i].descriptionMD;
		ListView_InsertItem(h32XRegList, &lvItem);


		if (_32X_register[i].side & 1)
		{
			adr = 0xA15100;
			adr |= _32X_register[i].adr;

			lvItem.iSubItem = 1;
			wsprintf(buf, "0x%0.6X", adr);
			lvItem.pszText = buf;
			ListView_SetItem(h32XRegList, &lvItem);

		}

		if (_32X_register[i].side & 2)
		{
			adr = 0x4000;
			adr |= _32X_register[i].adr;

			lvItem.iSubItem = 4;
			wsprintf(buf, "0x%0.6X", adr);
			lvItem.pszText = buf;
			ListView_SetItem(h32XRegList, &lvItem);

		}


		lvItem.iSubItem = 5;
		lvItem.pszText = _32X_register[i].description32X;
		ListView_SetItem(h32XRegList, &lvItem);

		i++;

	}
	ListView_Scroll(h32XRegList, 100, 0);
}

void Update32X_Reg_KMod()
{
	int         i;
	char		buf[64];
	LVITEM		lvItem = {0};
	WORD value;


	lvItem.mask = LVIF_TEXT;


	i = 0;
	while (_32X_register[i].side != 0xff)
	{
		lvItem.iItem = i;
		if (_32X_register[i].side & 1)
		{
			if (Peek32XRegister(_32X_register[i].adr, FALSE, &value))
				wsprintf(buf, "0x%0.4X", value);
			else lstrcpy(buf, "--");
			lvItem.iSubItem = 2;
			lvItem.pszText = buf;
			ListView_SetItem(h32XRegList, &lvItem);
		}

		if (_32X_register[i].side & 2)
		{
			if (Peek32XRegister(_32X_register[i].adr, TRUE, &value))
				wsprintf(buf, "0x%0.4X", value);
			else lstrcpy(buf, "--");
			lvItem.iSubItem = 3;
			lvItem.pszText = buf;
			ListView_SetItem(h32XRegList, &lvItem);
		}

		i++;

	}
}


BOOL CALLBACK _32X_RegDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	switch (Message)
	{
	case WM_INITDIALOG:
		h32X_Reg = hwnd;
		HandleWindow_KMod[DMODE_32_REG - 1] = hwnd;
		s32xreg_reset();
		{
			RECT rect;
			GetWindowRect(hwnd, &rect);
			minimum32XRegSize.cx = rect.right - rect.left;
			minimum32XRegSize.cy = rect.bottom - rect.top;
		}
		Resize32XRegList(hwnd);
		break;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) Resize32XRegList(hwnd);
		break;

	case WM_GETMINMAXINFO:
		if (minimum32XRegSize.cx && minimum32XRegSize.cy)
		{
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = minimum32XRegSize.cx;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = minimum32XRegSize.cy;
		}
		break;

	case WM_CLOSE:
		CloseWindow_KMod(DMODE_32_REG);
		break;

	case WM_DESTROY:
		h32X_Reg = NULL;
		h32XRegList = NULL;
		HandleWindow_KMod[DMODE_32_REG - 1] = NULL;
		OpenedWindow_KMod[DMODE_32_REG - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


void s32xreg_create(HINSTANCE hInstance, HWND hWndParent)
{
	minimum32XRegSize.cx = minimum32XRegSize.cy = 0;
	h32X_Reg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUG32X_REG), hWndParent, _32X_RegDlgProc);
}

void s32xreg_show(BOOL visibility)
{
	ShowWindow(h32X_Reg, visibility ? SW_SHOW : SW_HIDE);
}

void s32xreg_update()
{
	if (OpenedWindow_KMod[DMODE_32_REG - 1] == FALSE)	return;

	Update32X_Reg_KMod();
}

void s32xreg_reset()
{
	if (!h32X_Reg) return;
	_32X_RegInit_KMod(h32X_Reg);
	Update32X_Reg_KMod();
}
void s32xreg_destroy()
{
	if (h32X_Reg) DestroyWindow(h32X_Reg);
}

void s32xreg_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "32XRegOpen",
		OpenedWindow_KMod[DMODE_32_REG - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(h32X_Reg, "32XRegRect", config_file);
}

void s32xreg_restore_window(const char *config_file)
{
	BOOL visible = GetPrivateProfileInt("DebugWindows", "32XRegOpen", 0, config_file) != 0;
	DebugWindow_RestoreGeometry(h32X_Reg, "32XRegRect", config_file);
	OpenedWindow_KMod[DMODE_32_REG - 1] = visible && h32X_Reg != NULL;
	s32xreg_show(visible);
}
