#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"

#include "../mem_Z80.h"
#include "../z80.h"
#include "../z80dis.h"
#include "../Mem_M68k.h" //for Z80_State
#include "../G_gfx.h" //used for Put_Info

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "z80.h"
#include "../Cpu_Z80.h"
#include "../vdp_io.h"
#include "cpu_usage_graph.h"
#include "window_geometry.h"

static HWND hZ80;
static BOOL Z80_ViewMode;
static unsigned int  Z80_StartLineDisasm, Z80_StartLineMem;
static CHAR debug_string[1024];
HFONT hFont;

void SwitchZ80ViewMode_KMod()
{
	SCROLLINFO si;

	ZeroMemory(&si, sizeof(SCROLLINFO));
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nPage = 13;

	if (Z80_ViewMode)
	{
		// MEM VIEW
		si.nPos = Z80_StartLineMem;
		si.nMax = (1024) - 1;
	}
	else
	{
		// DISASM VIEW
		si.nPos = Z80_StartLineDisasm;
		si.nMax = (1024 * 8) - 1;
	}
	SetScrollInfo(GetDlgItem(hZ80, IDC_Z80_SCROLL), SB_CTL, &si, TRUE);
}

void UpdateZ80_KMod()
{
	unsigned int i, PC;
	unsigned char tmp_string[256];
	InvalidateRect(GetDlgItem(hZ80, IDC_Z80_USAGE), NULL, FALSE);
	
	SendDlgItemMessage(hZ80, IDC_Z80_DISAM, LB_RESETCONTENT, (WPARAM)0, (LPARAM)0);
	if (Z80_ViewMode == 0)
	{
		Z80_StartLineDisasm = GetScrollPos(GetDlgItem(hZ80, IDC_Z80_SCROLL), SB_CTL);
		PC = Z80_StartLineDisasm; //z80_Get_PC(&M_Z80);
		for (i = 0; i < 13; i++)
		{
			z80dis((unsigned char *)Ram_Z80, (int *)&PC, tmp_string);
			// to skip the \n z80dis add
			lstrcpyn(debug_string, tmp_string, lstrlen(tmp_string));
			SendDlgItemMessage(hZ80, IDC_Z80_DISAM, LB_INSERTSTRING, i, (LPARAM)debug_string);
		}
	}
	else
	{
		Z80_StartLineMem = GetScrollPos(GetDlgItem(hZ80, IDC_Z80_SCROLL), SB_CTL);
		for (i = 0; i < 13; i++)
		{
			wsprintf(tmp_string, "%.4X ", Z80_StartLineMem * 8 + i * 8);
			tmp_string[4] = 0x20;
			Hexview((unsigned char *)(Ram_Z80 + Z80_StartLineMem * 8 + i * 8), tmp_string + 5);
			tmp_string[22] = 0x20;
			Ansiview((unsigned char *)(Ram_Z80 + Z80_StartLineMem * 8 + i * 8), tmp_string + 23);
			wsprintf(debug_string, "%s", tmp_string);
			SendDlgItemMessage(hZ80, IDC_Z80_DISAM, LB_INSERTSTRING, i, (LPARAM)debug_string);
		}
	}


	wsprintf(debug_string, "AF =%.4X BC =%.4X DE =%.4X HL =%.4X\nAF2=%.4X BC2=%.4X DE2=%.4X HL2=%.4X", z80_Get_AF(&M_Z80), M_Z80.BC.w.BC, M_Z80.DE.w.DE, M_Z80.HL.w.HL, z80_Get_AF2(&M_Z80), M_Z80.BC2.w.BC2, M_Z80.DE2.w.DE2, M_Z80.HL2.w.HL2);
	SendDlgItemMessage(hZ80, IDC_Z80_STATUS_RS1, WM_SETTEXT, 0, (LPARAM)debug_string);

	wsprintf(debug_string, "IX =%.4X IY =%.4X SP =%.4X ", M_Z80.IX.w.IX, M_Z80.IY.w.IY, M_Z80.SP.w.SP);
	SendDlgItemMessage(hZ80, IDC_Z80_STATUS_RS3, WM_SETTEXT, 0, (LPARAM)debug_string);

	wsprintf(debug_string, "IFF1=%d IFF2=%d I=%.2X R=%.2X IM=%.2X", M_Z80.IFF.b.IFF1, M_Z80.IFF.b.IFF2, M_Z80.I, M_Z80.R.b.R1, M_Z80.IM);
	SendDlgItemMessage(hZ80, IDC_Z80_STATUS_MISC, WM_SETTEXT, 0, (LPARAM)debug_string);

	wsprintf(debug_string, "S=%d Z=%d Y=%d H=%d X=%d P=%d N=%d C=%d", (z80_Get_AF(&M_Z80) & 0x80) >> 7, (z80_Get_AF(&M_Z80) & 0x40) >> 6, (z80_Get_AF(&M_Z80) & 0x20) >> 5, (z80_Get_AF(&M_Z80) & 0x10) >> 4, (z80_Get_AF(&M_Z80) & 0x08) >> 3, (z80_Get_AF(&M_Z80) & 0x04) >> 2, (z80_Get_AF(&M_Z80) & 0x02) >> 1, (z80_Get_AF(&M_Z80) & 0x01) >> 0);
	SendDlgItemMessage(hZ80, IDC_Z80_STATUS_FLAG, WM_SETTEXT, 0, (LPARAM)debug_string);

	wsprintf(debug_string, "Bank = %0.8X State=%.2X", Bank_Z80, Z80_State);
	SendDlgItemMessage(hZ80, IDC_Z80_STATUS_BANK, WM_SETTEXT, 0, (LPARAM)debug_string);

	wsprintf(debug_string, "PC=%.4X", z80_Get_PC(&M_Z80));
	SendDlgItemMessage(hZ80, IDC_Z80_PC, WM_SETTEXT, 0, (LPARAM)debug_string);

	/*
	sprintf(GString, "Status=%.2X ILine=%.2X IVect=%.2X\n", M_Z80.Status & 0xFF, M_Z80.IntLine, M_Z80.IntVect);
	sprintf(GString, "Bank68K=%.8X State=%.2X\n", M_Z80.Status & 0xFF, Bank_M68K, Z80_State);
	*/

}

void DumpZ80_KMod(HWND hwnd)
{
	OPENFILENAME szFile;
	char szFileName[MAX_PATH];
	HANDLE hFr;
	DWORD dwBytesToWrite, dwBytesWritten;

	ZeroMemory(&szFile, sizeof(szFile));
	szFileName[0] = 0;  /*WITHOUT THIS, CRASH */

	strcpy(szFileName, Rom_Name);
	strcat(szFileName, "_Z80");

	szFile.lStructSize = sizeof(szFile);
	szFile.hwndOwner = hwnd;
	szFile.lpstrFilter = "RAM dump (*.ram)\0*.ram\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Dump Z80 memory";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "ram";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	hFr = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFr == INVALID_HANDLE_VALUE)
		return;

	dwBytesToWrite = 8 * 1024;
	WriteFile(hFr, Ram_Z80, dwBytesToWrite, &dwBytesWritten, NULL);

	CloseHandle(hFr);

	Put_Info("Z80 Ram dumped", 1500);
}

/* Fit the actual fixed font before placing the graph beside the registers. */
static void Z80_FitStatus(HWND hwnd)
{
    const int ids[5] = {IDC_Z80_STATUS_RS1,IDC_Z80_STATUS_RS3,IDC_Z80_STATUS_MISC,IDC_Z80_STATUS_FLAG,IDC_Z80_STATUS_BANK};
    const char *samples[5] = {
        "AF =FFFF BC =FFFF DE =FFFF HL =FFFF",
        "IX =FFFF IY =FFFF SP =FFFF",
        "IFF1=1 IFF2=1 I=FF R=FF IM=02",
        "S=1 Z=1 Y=1 H=1 X=1 P=1 N=1 C=1",
        "Bank = FFFFFFFF State=03"
    };
    RECT r, group, client, outer;
    HDC dc = GetDC(hwnd);
    int i, top, right = 0, dx, dy;
    GetWindowRect(GetDlgItem(hwnd, ids[0]), &r);
    MapWindowPoints(NULL, hwnd, (POINT *)&r, 2);
    top = r.top;
    for (i = 0; i < 5; ++i) {
        TEXTMETRIC metrics;
        SIZE extent;
        HFONT previous = (HFONT)SelectObject(dc,
            (HFONT)SendDlgItemMessage(hwnd, ids[i], WM_GETFONT, 0, 0));
        GetTextMetrics(dc, &metrics);
        GetTextExtentPoint32(dc, samples[i], lstrlen(samples[i]), &extent);
        SelectObject(dc, previous);
        GetWindowRect(GetDlgItem(hwnd, ids[i]), &r);
        MapWindowPoints(NULL, hwnd, (POINT *)&r, 2);
        MoveWindow(GetDlgItem(hwnd, ids[i]), r.left, top, extent.cx + 4,
            metrics.tmHeight * (i ? 1 : 2) + 2, FALSE);
        right = max(right, r.left + extent.cx + 4);
        top += metrics.tmHeight * (i ? 1 : 2) + 6;
    }
    ReleaseDC(hwnd, dc);
    GetWindowRect(GetDlgItem(hwnd, IDC_Z80_USAGE_GROUP), &group);
    MapWindowPoints(NULL, hwnd, (POINT *)&group, 2);
    dx = max(0, right + 8 - group.left);
    dy = max(0, top + 4 - group.bottom);
    for (i = IDC_Z80_USAGE; i <= IDC_Z80_USAGE_NOTE; ++i) {
        GetWindowRect(GetDlgItem(hwnd, i), &r);
        MapWindowPoints(NULL, hwnd, (POINT *)&r, 2);
        MoveWindow(GetDlgItem(hwnd, i), r.left + dx, r.top,
            r.right - r.left, r.bottom - r.top + (i == IDC_Z80_USAGE_GROUP ? dy : 0), FALSE);
    }
    GetClientRect(hwnd, &client);
    GetWindowRect(hwnd, &outer);
    SetWindowPos(hwnd, NULL, 0, 0,
        outer.right - outer.left + max(0, group.right + dx + 8 - client.right),
        outer.bottom - outer.top + max(0, group.bottom + dy + 8 - client.bottom),
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

BOOL CALLBACK Z80DlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	
	SCROLLINFO si;

	switch (Message)
	{
	case WM_INITDIALOG:
		hZ80 = hwnd;
		HandleWindow_KMod[DMODE_Z80 - 1] = hwnd;
		hFont = (HFONT)GetStockObject(OEM_FIXED_FONT);
		SendDlgItemMessage(hwnd, IDC_Z80_DISAM, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendDlgItemMessage(hwnd, IDC_Z80_STATUS_RS1, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendDlgItemMessage(hwnd, IDC_Z80_STATUS_RS2, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendDlgItemMessage(hwnd, IDC_Z80_STATUS_RS3, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendDlgItemMessage(hwnd, IDC_Z80_STATUS_MISC, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendDlgItemMessage(hwnd, IDC_Z80_STATUS_FLAG, WM_SETFONT, (WPARAM)hFont, TRUE);
		SendDlgItemMessage(hwnd, IDC_Z80_STATUS_BANK, WM_SETFONT, (WPARAM)hFont, TRUE);
		Z80_FitStatus(hwnd);
		z80debug_reset();
		break;

    case WM_DRAWITEM:
        if (wParam == IDC_Z80_USAGE) {
            unsigned long values[Z80_USAGE_HISTORY];
            unsigned count = (Genesis_Started || SegaCD_Started || _32X_Started) ? Z80_Usage_GetHistory(values) : 0;
            CPUUsage_DrawGraph((DRAWITEMSTRUCT *)lParam, values, count);
            return TRUE;
        }
        return FALSE;

	case WM_SHOWWINDOW:
		SwitchZ80ViewMode_KMod();
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_Z80_DUMP_MEM:
			DumpZ80_KMod(hwnd);
			break;

		case IDC_Z80_VIEW_MEM:
			Z80_ViewMode = !Z80_ViewMode;
			if (Z80_ViewMode == 0)
			{
				SendDlgItemMessage(hwnd, IDC_Z80_VIEW_MEM, WM_SETTEXT, (WPARAM)0, (LPARAM)"View Memory");
			}
			else
			{
				SendDlgItemMessage(hwnd, IDC_Z80_VIEW_MEM, WM_SETTEXT, (WPARAM)0, (LPARAM)"View Disasm");
			}
			SwitchZ80ViewMode_KMod();
			UpdateWindow(hwnd);
			UpdateZ80_KMod();
			break;

		case IDC_Z80_PC:
			if (Z80_ViewMode == 0)
			{
				Z80_StartLineDisasm = z80_Get_PC(&M_Z80);
				SwitchZ80ViewMode_KMod();
				UpdateWindow(hwnd);
				UpdateZ80_KMod();
			}
			break;
		}

		break;

	case WM_VSCROLL:
		ZeroMemory(&si, sizeof(SCROLLINFO));
		si.cbSize = sizeof(si);
		si.fMask = SIF_ALL;
		GetScrollInfo((HWND)lParam, SB_CTL, &si);
		switch (LOWORD(wParam))
		{
		case SB_PAGEUP:
			si.nPos -= si.nPage;
			break;
		case SB_PAGEDOWN:
			si.nPos += si.nPage;
			break;
		case SB_LINEUP:
			si.nPos--;
			break;
		case SB_LINEDOWN:
			si.nPos++;
			break;
		case SB_THUMBTRACK:
			//si.nPos = HIWORD(wParam);
			si.nPos = si.nTrackPos;
			break;
		}

		si.cbSize = sizeof(si);
		si.fMask = SIF_POS;
		SetScrollInfo((HWND)lParam, SB_CTL, &si, TRUE);
		UpdateWindow(hwnd);
		UpdateZ80_KMod();
		return 0;


	case WM_CLOSE:
		CloseWindow_KMod(DMODE_Z80);
		break;

	case WM_DESTROY:
		hZ80 = NULL;
		HandleWindow_KMod[DMODE_Z80 - 1] = NULL;
		OpenedWindow_KMod[DMODE_Z80 - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

void z80debug_create(HINSTANCE hInstance, HWND hWndParent)
{
	hZ80 = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGZ80), hWndParent, Z80DlgProc);
}

void z80debug_show(BOOL visibility)
{
	ShowWindow(hZ80, visibility ? SW_SHOW : SW_HIDE);
}

void z80debug_update()
{
	if (OpenedWindow_KMod[DMODE_Z80-1] == FALSE)	return;

	UpdateZ80_KMod();
}

void z80debug_reset()
{
	Z80_ViewMode = 0; //disasm
	Z80_StartLineDisasm = Z80_StartLineMem = 0;
	SwitchZ80ViewMode_KMod(); // init with wrong values (since no game loaded by default)
}
void z80debug_destroy()
{
	if (hZ80) DestroyWindow(hZ80);
}

void z80debug_dump()
{
	DumpZ80_KMod(hZ80);
}
void z80debug_save_window(const char *config_file)
{
    WritePrivateProfileString("DebugWindows", "Z80Open",
        OpenedWindow_KMod[DMODE_Z80 - 1] ? "1" : "0", config_file);
    DebugWindow_SaveGeometry(hZ80, "Z80Rect", config_file);
}

void z80debug_restore_window(const char *config_file)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "Z80Open", 0, config_file) != 0;
    if (!hZ80) return;
    GetWindowRect(hZ80, &rect);
    DebugWindow_RestoreGeometry(hZ80, "Z80Rect", config_file);
    /* Keep the size measured from the current font and graph layout. */
    SetWindowPos(hZ80, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_Z80 - 1] = visible;
    z80debug_show(visible);
}
