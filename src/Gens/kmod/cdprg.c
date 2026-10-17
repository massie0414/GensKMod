#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../Mem_S68k.h"
#include "common.h"
#include "cdprg.h"
#include "window_geometry.h"
#include "vdp.h"

static HWND hCD_PRG;
// 8192 8x16 tiles cover all 512 KiB of Program RAM.
#define CD_PRG_COLUMNS 128
#define CD_PRG_ROWS 64
#define CD_PRG_WIDTH (CD_PRG_COLUMNS * 8)
#define CD_PRG_HEIGHT (CD_PRG_ROWS * 16)
#define CD_PRG_PREVIEW_ID 50990
#define CD_PRG_PREVIEW_WIDTH 64
#define CD_PRG_PREVIEW_HEIGHT 128

static unsigned int programSelectedTile;
static WNDPROC programImageProc;

static unsigned char CD_PRGPixel(unsigned int tile, unsigned int x, unsigned int y)
{
	unsigned char data = Ram_Prg[tile * 64 + y * 4 + ((x / 2) ^ 1)];
	return (x & 1) ? (data & 15) : (data >> 4);
}

static void CD_PRGSetSelectedTile(HWND hwnd, unsigned int tile)
{
	char address[32];
	if (tile >= CD_PRG_COLUMNS * CD_PRG_ROWS) return;
	programSelectedTile = tile;
	wsprintf(address, "0x%06X", programSelectedTile * 64);
	SetDlgItemText(hwnd, IDC_CD_GFX_ADR, address);
	InvalidateRect(GetDlgItem(hwnd, IDC_CD_GFX), NULL, FALSE);
	InvalidateRect(GetDlgItem(hwnd, CD_PRG_PREVIEW_ID), NULL, FALSE);
}

static BOOL CD_PRGNavigate(HWND hwnd, WPARAM key)
{
	if (key == VK_LEFT)
	{
		if (programSelectedTile > 0) CD_PRGSetSelectedTile(hwnd, programSelectedTile - 1);
		return TRUE;
	}
	if (key == VK_RIGHT)
	{
		CD_PRGSetSelectedTile(hwnd, programSelectedTile + 1);
		return TRUE;
	}
	if (key == VK_UP)
	{
		if (programSelectedTile >= CD_PRG_COLUMNS)
			CD_PRGSetSelectedTile(hwnd, programSelectedTile - CD_PRG_COLUMNS);
		return TRUE;
	}
	if (key == VK_DOWN)
	{
		CD_PRGSetSelectedTile(hwnd, programSelectedTile + CD_PRG_COLUMNS);
		return TRUE;
	}
	return FALSE;
}

static void CD_PRGSelectTile(HWND hwnd, int x, int y)
{
	RECT rect;
	GetClientRect(GetDlgItem(hwnd, IDC_CD_GFX), &rect);
	if (x < 0 || y < 0 || x >= rect.right || y >= rect.bottom) return;
	CD_PRGSetSelectedTile(hwnd, (y * CD_PRG_HEIGHT / rect.bottom / 16) * CD_PRG_COLUMNS +
		x * CD_PRG_WIDTH / rect.right / 8);
}

static LRESULT CALLBACK CD_PRGImageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_LBUTTONDOWN)
	{
		SetFocus(hwnd);
		CD_PRGSelectTile(GetParent(hwnd), (short)LOWORD(lParam), (short)HIWORD(lParam));
		return 0;
	}
	if (message == WM_GETDLGCODE)
		return CallWindowProc(programImageProc, hwnd, message, wParam, lParam) | DLGC_WANTARROWS;
	if (message == WM_KEYDOWN && CD_PRGNavigate(GetParent(hwnd), wParam)) return 0;
	return CallWindowProc(programImageProc, hwnd, message, wParam, lParam);
}

static void CD_PRGDrawPreview(LPDRAWITEMSTRUCT draw)
{
	DWORD pixels[8 * 16];
	BITMAPINFO bmi;
	unsigned int x, y;
	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++)
		{
			COLORREF color = vdpdebug_getColor(pal_KMod, CD_PRGPixel(programSelectedTile, x, y));
			pixels[y * 8 + x] = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
		}
	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = 8;
	bmi.bmiHeader.biHeight = -16;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	SetStretchBltMode(draw->hDC, COLORONCOLOR);
	StretchDIBits(draw->hDC, 0, 0, draw->rcItem.right, draw->rcItem.bottom,
		0, 0, 8, 16, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}





void DrawCD_PRG_KMod(LPDRAWITEMSTRUCT hlDIS)
{
	static DWORD pixels[CD_PRG_WIDTH * CD_PRG_HEIGHT];
	DWORD colors[16];
	BITMAPINFO bmi;
	unsigned int tile, x, y, i;

	for (i = 0; i < 16; ++i)
	{
		COLORREF color = vdpdebug_getColor(pal_KMod, i);
		colors[i] = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
	}

	// Write pixels directly instead of allocating a GDI bitmap for every tile.
	for (tile = 0; tile < CD_PRG_COLUMNS * CD_PRG_ROWS; ++tile)
	{
		unsigned int tileX = (tile % CD_PRG_COLUMNS) * 8;
		unsigned int tileY = (tile / CD_PRG_COLUMNS) * 16;
		for (y = 0; y < 16; ++y)
		{
			for (x = 0; x < 8; x += 2)
			{
				unsigned char data = Ram_Prg[tile * 64 + y * 4 + ((x / 2) ^ 1)];
				pixels[(tileY + y) * CD_PRG_WIDTH + tileX + x] = colors[data >> 4];
				pixels[(tileY + y) * CD_PRG_WIDTH + tileX + x + 1] = colors[data & 15];
			}
		}
	}

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = CD_PRG_WIDTH;
	bmi.bmiHeader.biHeight = -CD_PRG_HEIGHT;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	SetStretchBltMode(hlDIS->hDC, COLORONCOLOR);
	StretchDIBits(hlDIS->hDC, hlDIS->rcItem.left, hlDIS->rcItem.top,
		hlDIS->rcItem.right - hlDIS->rcItem.left, hlDIS->rcItem.bottom - hlDIS->rcItem.top,
		0, 0, CD_PRG_WIDTH, CD_PRG_HEIGHT, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
	{
		RECT selection;
		int width = hlDIS->rcItem.right - hlDIS->rcItem.left;
		int height = hlDIS->rcItem.bottom - hlDIS->rcItem.top;
		int x = programSelectedTile % CD_PRG_COLUMNS;
		int y = programSelectedTile / CD_PRG_COLUMNS;
		selection.left = hlDIS->rcItem.left + MulDiv(x * 8, width, CD_PRG_WIDTH);
		selection.top = hlDIS->rcItem.top + MulDiv(y * 16, height, CD_PRG_HEIGHT);
		selection.right = hlDIS->rcItem.left + MulDiv((x + 1) * 8, width, CD_PRG_WIDTH);
		selection.bottom = hlDIS->rcItem.top + MulDiv((y + 1) * 16, height, CD_PRG_HEIGHT);
		FrameRect(hlDIS->hDC, &selection, (HBRUSH)GetStockObject(WHITE_BRUSH));
	}
}


void UpdateCD_PRG_KMod()
{
	if (OpenedWindow_KMod[DMODE_CD_PRG - 1] == FALSE) return;
	RedrawWindow(GetDlgItem(hCD_PRG, IDC_CD_GFX), NULL, NULL, RDW_INVALIDATE);
	InvalidateRect(GetDlgItem(hCD_PRG, CD_PRG_PREVIEW_ID), NULL, FALSE);
}


void CD_PRGInit_KMod(HWND hwnd)
{
	RECT image, frame = {0, 0, 0, 0}, window;
	MONITORINFO monitor;
	int width = CD_PRG_WIDTH, height = CD_PRG_HEIGHT;
	int margin, clientWidth, clientHeight;

	GetWindowRect(GetDlgItem(hwnd, IDC_CD_GFX), &image);
	MapWindowPoints(NULL, hwnd, (LPPOINT)&image, 2);
	margin = image.left;
	AdjustWindowRectEx(&frame, GetWindowLong(hwnd, GWL_STYLE), FALSE,
		GetWindowLong(hwnd, GWL_EXSTYLE));
	monitor.cbSize = sizeof(monitor);
	if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
	{
		int availableWidth = monitor.rcWork.right - monitor.rcWork.left -
			(frame.right - frame.left) - margin * 2;
		int availableHeight = monitor.rcWork.bottom - monitor.rcWork.top -
			(frame.bottom - frame.top) - image.top - margin * 2 - CD_PRG_PREVIEW_HEIGHT;
		if (availableWidth < width && availableWidth > 0)
		{
			width = availableWidth;
			height = MulDiv(width, CD_PRG_HEIGHT, CD_PRG_WIDTH);
		}
		if (availableHeight < height && availableHeight > 0)
		{
			height = availableHeight;
			width = MulDiv(height, CD_PRG_WIDTH, CD_PRG_HEIGHT);
		}
	}

	clientWidth = width + margin * 2;
	clientHeight = image.top + height + margin * 2 + CD_PRG_PREVIEW_HEIGHT;
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX), margin, image.top, width, height, TRUE);
	MoveWindow(GetDlgItem(hwnd, CD_PRG_PREVIEW_ID), margin, image.top + height + margin,
		CD_PRG_PREVIEW_WIDTH, CD_PRG_PREVIEW_HEIGHT, TRUE);
	/* Give the address the remaining width, including room for A-F digits. */
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX_ADR), margin * 2 + CD_PRG_PREVIEW_WIDTH,
		image.top + height + margin, max(0, clientWidth - margin * 3 - CD_PRG_PREVIEW_WIDTH),
		CD_PRG_PREVIEW_HEIGHT, TRUE);
	GetWindowRect(hwnd, &window);
	window.right = clientWidth + frame.right - frame.left;
	window.bottom = clientHeight + frame.bottom - frame.top;
	if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
	{
		window.left = max(monitor.rcWork.left, min(window.left, monitor.rcWork.right - window.right));
		window.top = max(monitor.rcWork.top, min(window.top, monitor.rcWork.bottom - window.bottom));
	}
	SetWindowPos(hwnd, NULL, window.left, window.top, window.right, window.bottom,
		SWP_NOZORDER | SWP_NOACTIVATE);
}


BOOL CALLBACK CD_PRGDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_INITDIALOG:
		SetWindowText(hwnd, "CD Program RAM");
		programSelectedTile = 0;
		HandleWindow_KMod[DMODE_CD_PRG - 1] = hwnd;
		CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
			0, 0, 0, 0, hwnd, (HMENU)CD_PRG_PREVIEW_ID, GetModuleHandle(NULL), NULL);
		programImageProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwnd, IDC_CD_GFX), GWLP_WNDPROC,
			(LONG_PTR)CD_PRGImageProc);
		CD_PRGInit_KMod(hwnd);
		break;

	case WM_KEYDOWN:
		return CD_PRGNavigate(hwnd, wParam);

	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_CD_GFX)
			DrawCD_PRG_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == CD_PRG_PREVIEW_ID)
			CD_PRGDrawPreview((LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) != IDCANCEL) return FALSE;
	case WM_CLOSE:
		CloseWindow_KMod(DMODE_CD_PRG);
		break;

	case WM_DESTROY:
		hCD_PRG = NULL;
		HandleWindow_KMod[DMODE_CD_PRG - 1] = NULL;
		OpenedWindow_KMod[DMODE_CD_PRG - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

void cdprg_create(HINSTANCE hInstance, HWND hWndParent)
{
	hCD_PRG = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGCD_GFX), hWndParent, CD_PRGDlgProc);
}

void cdprg_show(BOOL visibility)
{
	ShowWindow(hCD_PRG, visibility ? SW_SHOW : SW_HIDE);
}

void cdprg_update()
{
	if (OpenedWindow_KMod[DMODE_CD_PRG-1] == FALSE)	return;

	UpdateCD_PRG_KMod();
}

void cdprg_reset()
{
	CD_PRGInit_KMod(hCD_PRG);
}
void cdprg_destroy()
{
	if (hCD_PRG) DestroyWindow(hCD_PRG);
}

void cdprg_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "CDPrgOpen",
		OpenedWindow_KMod[DMODE_CD_PRG - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(hCD_PRG, "CDPrgRect", config_file);
}

void cdprg_restore_window(const char *config_file)
{
	RECT rect;
	BOOL visible = GetPrivateProfileInt("DebugWindows", "CDPrgOpen", 0, config_file) != 0;
	if (!hCD_PRG) return;
	GetWindowRect(hCD_PRG, &rect);
	DebugWindow_RestoreGeometry(hCD_PRG, "CDPrgRect", config_file);
	/* Keep the image layout calculated for the current display. */
	SetWindowPos(hCD_PRG, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	OpenedWindow_KMod[DMODE_CD_PRG - 1] = visible;
	cdprg_show(visible);
}

