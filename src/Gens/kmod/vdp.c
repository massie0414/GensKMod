#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../G_main.h"
#include "../resource.h"
#include "../vdp_io.h"
#include "../vdp_rend.h"

#include "../G_gfx.h" //used for Put_Info

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "vdp.h"
#include "window_geometry.h"
UCHAR pal_KMod;
static HWND hVDP;
static SIZE minimumVDPSize;
static int selectedVDPSprite = -1;

void vdpdebug_select_sprite(int index)
{
	selectedVDPSprite = index >= 0 && index < 80 ? index : -1;
	if (hVDP) InvalidateRect(GetDlgItem(hVDP, IDC_VDP_TILES), NULL, FALSE);
}

static long palH, palV;
static UCHAR zoomTile_KMod;
static USHORT tile_KMod;
static BOOL hasSelectedTile;
static WNDPROC tileWindowProc;
static CHAR debug_string[1024];

static HWND hWndRedArrow;
static HWND hWndPal;
static RECT rcArrow, rcPal, rcTiles;

static const COLORREF cgaPal[16] = {
	RGB(0, 0, 0), RGB(0, 0, 0xAA), RGB(0, 0xAA, 0), RGB(0, 0xAA, 0xAA),
	RGB(0xAA, 0, 0), RGB(0xAA, 0, 0xAA), RGB(0xAA, 0x55, 0), RGB(0xAA, 0xAA, 0xAA),
	RGB(0x55, 0x55, 0x55), RGB(0x55, 0x55, 0xFF), RGB(0x55, 0xFF, 0x55), RGB(0x55, 0xFF, 0xFF),
	RGB(0xFF, 0x55, 0x55), RGB(0xFF, 0x55, 0xFF), RGB(0xFF, 0xFF, 0x55), RGB(0xFF, 0xFF, 0xFF),
};

static const COLORREF monoPal[16] = {
	RGB(0, 0, 0), RGB(0x11, 0x11, 0x11), RGB(0x22, 0x22, 0x22), RGB(0x33, 0x33, 0x33),
	RGB(0x44, 0x44, 0x44), RGB(0x55, 0x55, 0x55), RGB(0x66, 0x66, 0x66), RGB(0x77, 0x77, 0x77),
	RGB(0x88, 0x88, 0x88), RGB(0x99, 0x99, 0x99), RGB(0xAA, 0xAA, 0xAA), RGB(0xBB, 0xBB, 0xBB),
	RGB(0xCC, 0xCC, 0xCC), RGB(0xDD, 0xDD, 0xDD), RGB(0xEE, 0xEE, 0xEE), RGB(0xFF, 0xFF, 0xFF),
};

#define VDP_TILE_COLUMNS 64
#define VDP_TILE_ROWS 32

#define VDP_PALETTES 6
#define VDP_PALETTE_CGA 4
#define VDP_PALETTE_MONO 5

void UpdateVDP_KMod()
{
	RedrawWindow(hWndPal, NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(GetDlgItem(hVDP, IDC_VDP_TILES), NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(GetDlgItem(hVDP, IDC_VDP_PREVIEW), NULL, NULL, RDW_INVALIDATE);

}

void DrawVDP_Pal_KMod(LPDRAWITEMSTRUCT hlDIS)
{
	unsigned char i, j;
	HBRUSH newBrush = NULL;
	RECT rc;

	for (j = 0; j < VDP_PALETTES; j++)
	{
		rc.top = j* palV;
		rc.bottom = rc.top + palV;

		for (i = 0; i < 16; i++)
		{
			if (newBrush)	DeleteObject((HGDIOBJ)newBrush);
			rc.left = i*palH;
			rc.right = rc.left + palH;

			newBrush = CreateSolidBrush(vdpdebug_getColor(j, i));

			FillRect(hlDIS->hDC, &rc, newBrush);
		}
	}

	if (newBrush)	DeleteObject((HGDIOBJ)newBrush);

	MoveWindow(hWndRedArrow, rcPal.left - (rcArrow.right - rcArrow.left), rcPal.top + pal_KMod*palV + (palV - (rcArrow.bottom - rcArrow.top)) / 2, (rcArrow.right - rcArrow.left), (rcArrow.bottom - rcArrow.top), TRUE);
}


void DrawVDP_Tiles_KMod(LPDRAWITEMSTRUCT hlDIS)
{

	WORD posX, posY, maxY, maxX;
	unsigned short int numTile = 0;

	HDC hDC;
	HBITMAP hBitmap, hOldBitmap;

	hDC = CreateCompatibleDC(hlDIS->hDC);
	hBitmap = CreateCompatibleBitmap(hlDIS->hDC, hlDIS->rcItem.right - hlDIS->rcItem.left, hlDIS->rcItem.bottom - hlDIS->rcItem.top);
	hOldBitmap = SelectObject(hDC, hBitmap);

	maxY = VDP_TILE_ROWS * 8;
	maxX = VDP_TILE_COLUMNS * 8;

	for (posY = 0; posY < maxY; posY += 8)
	{
		for (posX = 0; posX < maxX; posX += 8)
		{
			vdpdebug_drawTile(hDC, numTile++, posX, posY, pal_KMod, 1);
		}
	}

	if (selectedVDPSprite >= 0)
	{
		unsigned int base = (VDP_Reg.Spr_Att_Adr & ((VDP_Reg.Set4 & 1) ? 0x7E : 0x7F)) << 9;
		unsigned short data[4];
		int i, count, first, units = (VDP_Reg.Set4 & 6) == 6 ? 2 : 1;
		for (i = 0; i < 4; i++)
			data[i] = *(unsigned short *)&VRam[(base + selectedVDPSprite * 8 + i * 2) & 0xFFFF];
		first = (data[2] & 0x7FF) * units;
		count = (((data[1] >> 10) & 3) + 1) * (((data[1] >> 8) & 3) + 1) * units;
		if (data[0] || data[1] || data[2] || data[3])
		{
			HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
			for (i = 0; i < count; i++)
			{
				int tile = (first + i) & 0x7FF;
				RECT frame;
				SetRect(&frame, (tile % VDP_TILE_COLUMNS) * 8, (tile / VDP_TILE_COLUMNS) * 8,
					(tile % VDP_TILE_COLUMNS) * 8 + 8, (tile / VDP_TILE_COLUMNS) * 8 + 8);
				FrameRect(hDC, &frame, red);
			}
			DeleteObject(red);
		}
	}

	/* Outline the full 8x8 tile, including its outermost pixels. */
	if (hasSelectedTile)
	{
		RECT frame;
		HBRUSH yellow = CreateSolidBrush(RGB(255, 255, 0));
		int x = (tile_KMod % VDP_TILE_COLUMNS) * 8;
		int y = (tile_KMod / VDP_TILE_COLUMNS) * 8;
		SetRect(&frame, x, y, x + 8, y + 8);
		FrameRect(hDC, &frame, yellow);
		DeleteObject(yellow);
	}

	BitBlt(
		hlDIS->hDC, // handle to destination device context
		0,  // x-coordinate of destination rectangle's upper-left
		// corner
		0,  // y-coordinate of destination rectangle's upper-left
		// corner
		maxX,  // width of destination rectangle
		maxY, // height of destination rectangle
		hDC,  // handle to source device context
		0,   // x-coordinate of source rectangle's upper-left
		// corner
		0,   // y-coordinate of source rectangle's upper-left
		// corner
		SRCCOPY  // raster operation code
		);

	SelectObject(hDC, hOldBitmap);
	DeleteObject(hBitmap);

	DeleteDC(hDC);
}

void DrawVDPZoom_Tiles_KMod(LPDRAWITEMSTRUCT hlDIS)
{
	vdpdebug_drawTile(hlDIS->hDC, tile_KMod, 0, 0, pal_KMod, zoomTile_KMod);
}

void DumpTiles_KMod(HWND hwnd)
{

	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmiHeader;
	RGBQUAD				bmiColors[16];
	LPBYTE				pBits;

	OPENFILENAME szFile;
	char szFileName[MAX_PATH];
	HANDLE hFr;
	DWORD dwBytesToWrite, dwBytesWritten;

	WORD posX, posY;

	COLORREF		tmpColor;
	DWORD			j;

	unsigned short int numTile;
	unsigned char		TileData;


	ZeroMemory(&szFile, sizeof(szFile));
	szFileName[0] = 0;  /*WITHOUT THIS, CRASH */

	strcpy(szFileName, Rom_Name);
	strcat(szFileName, "_VRAM");

	szFile.lStructSize = sizeof(szFile);
	szFile.hwndOwner = hwnd;
	szFile.lpstrFilter = "16Colors Bitmap file (*.bmp)\0*.bmp\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Convert VRAM to bitmap";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "bmp";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	bmfh.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"
	bmfh.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD) + 64 * 2000 / 2); // 4bits per pixel
	bmfh.bfOffBits = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD));


	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth = 16 * 8;
	bmiHeader.biHeight = 125 * 8;
	bmiHeader.biPlanes = 0;
	bmiHeader.biBitCount = 4;
	bmiHeader.biClrUsed = 16;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 64 * 2000 / 2;
	bmiHeader.biClrImportant = 0;

	for (j = 0; j < 16; j++)
	{
		tmpColor = vdpdebug_getColor(pal_KMod, j);
		bmiColors[j].rgbRed = (BYTE)tmpColor & 0xFF;
		tmpColor >>= 8;
		bmiColors[j].rgbGreen = (BYTE)tmpColor & 0xFF;
		tmpColor >>= 8;
		bmiColors[j].rgbBlue = (BYTE)tmpColor & 0xFF;
	}

	pBits = (LPBYTE)LocalAlloc(LPTR, bmiHeader.biSizeImage);
	if (pBits == NULL)
	{
		return;
	}


	numTile = 0;
	for (posY = 0; posY < 125; posY++)
	{
		for (posX = 0; posX < 16; posX++)
		{
			for (j = 0; j < 8; j++)
			{
				TileData = VRam[numTile * 32 + j * 4 + 1];
				pBits[posX * 4 + (124 - posY) * 8 * 16 * 4 + (7 - j) * 16 * 4] = TileData;

				TileData = VRam[numTile * 32 + j * 4];
				pBits[posX * 4 + (124 - posY) * 8 * 16 * 4 + (7 - j) * 16 * 4 + 1] = TileData;

				TileData = VRam[numTile * 32 + j * 4 + 3];
				pBits[posX * 4 + (124 - posY) * 8 * 16 * 4 + (7 - j) * 16 * 4 + 2] = TileData;

				TileData = VRam[numTile * 32 + j * 4 + 2];
				pBits[posX * 4 + (124 - posY) * 8 * 16 * 4 + (7 - j) * 16 * 4 + 3] = TileData;
			}
			numTile++;
		}
	}

	hFr = CreateFile(szFileName, GENERIC_WRITE, (DWORD)0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, (HANDLE)NULL);
	if (hFr == INVALID_HANDLE_VALUE)
	{
		LocalFree((HLOCAL)pBits);
		return;
	}

	dwBytesToWrite = sizeof(BITMAPFILEHEADER);
	WriteFile(hFr, &bmfh, dwBytesToWrite, &dwBytesWritten, NULL);

	dwBytesToWrite = sizeof(BITMAPINFOHEADER);
	WriteFile(hFr, &bmiHeader, dwBytesToWrite, &dwBytesWritten, NULL);

	dwBytesToWrite = 16 * sizeof(RGBQUAD);
	WriteFile(hFr, bmiColors, dwBytesToWrite, &dwBytesWritten, NULL);

	dwBytesToWrite = bmiHeader.biSizeImage;
	WriteFile(hFr, pBits, dwBytesToWrite, &dwBytesWritten, NULL);

	CloseHandle(hFr);

	LocalFree((HLOCAL)pBits);

	Put_Info("Tiles dumped", 1500);
}

void DumpVRAM_KMod(HWND hwnd)
{
	OPENFILENAME szFile;
	char szFileName[MAX_PATH];
	HANDLE hFr;
	DWORD dwBytesToWrite, dwBytesWritten;

	ZeroMemory(&szFile, sizeof(szFile));
	szFileName[0] = 0;  /*WITHOUT THIS, CRASH */

	strcpy(szFileName, Rom_Name);
	strcat(szFileName, "_VRAM");

	szFile.lStructSize = sizeof(szFile);
	szFile.hwndOwner = hwnd;
	szFile.lpstrFilter = "RAM dump (*.ram)\0*.ram\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Dump VRAM";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "ram";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	hFr = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFr == INVALID_HANDLE_VALUE)
		return;

	dwBytesToWrite = 64 * 1024;
	WriteFile(hFr, VRam, dwBytesToWrite, &dwBytesWritten, NULL);

	CloseHandle(hFr);

	Put_Info("VRAM dumped", 1500);
}


void DumpCRAM_KMod(HWND hwnd)
{
	OPENFILENAME szFile;
	char szFileName[MAX_PATH];
	HANDLE hFr;
	DWORD dwBytesToWrite, dwBytesWritten;

	ZeroMemory(&szFile, sizeof(szFile));
	szFileName[0] = 0;  /*WITHOUT THIS, CRASH */

	strcpy(szFileName, Rom_Name);
	strcat(szFileName, "_CRAM");


	szFile.lStructSize = sizeof(szFile);
	szFile.hwndOwner = hwnd;
	szFile.lpstrFilter = "RAM dump (*.ram)\0*.ram\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Dump CRAM";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "ram";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	hFr = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFr == INVALID_HANDLE_VALUE)
		return;

	dwBytesToWrite = 64 * 8;
	WriteFile(hFr, CRam, dwBytesToWrite, &dwBytesWritten, NULL);

	CloseHandle(hFr);

	Put_Info("CRAM dumped", 1500);
}

void VDPPal_Choose()
{
	POINT pt;

	GetCursorPos(&pt);
	ScreenToClient(hVDP, &pt);
	if (!PtInRect(&rcPal, pt))        return;

	pt.y -= rcPal.top;
	pal_KMod = (unsigned char)(pt.y / palV);

	if (Paused)	UpdateVDP_KMod();
}

static void VDPSelectTile_KMod(POINT pt)
{
	if (!PtInRect(&rcTiles, pt)) return;

	pt.y -= rcTiles.top;
	tile_KMod = (unsigned char)(pt.y / 8);
	tile_KMod *= VDP_TILE_COLUMNS;


	pt.x -= rcTiles.left;
	tile_KMod += (unsigned char)(pt.x / 8);
	hasSelectedTile = TRUE;

	wsprintf(debug_string, "Tile 0x%.4X", tile_KMod);
	SendDlgItemMessage(hVDP, IDC_VDP_TILE_ZOOM_ADR, WM_SETTEXT, 0, (LPARAM)debug_string);

	if (Paused)	UpdateVDP_KMod();
}


void VDPTile_Choose()
{
	POINT pt;
	GetCursorPos(&pt);
	ScreenToClient(hVDP, &pt);
	VDPSelectTile_KMod(pt);
	SetFocus(GetDlgItem(hVDP, IDC_VDP_TILES));
}


static BOOL VDPNavigateTile_KMod(WPARAM key)
{
	int x = tile_KMod % VDP_TILE_COLUMNS;
	int y = tile_KMod / VDP_TILE_COLUMNS;
	POINT pt;
	switch (key)
	{
	case VK_LEFT:  if (x > 0) x--; break;
	case VK_RIGHT: if (x < VDP_TILE_COLUMNS - 1) x++; break;
	case VK_UP:    if (y > 0) y--; break;
	case VK_DOWN:  if (y < VDP_TILE_ROWS - 1) y++; break;
	default: return FALSE;
	}
	pt.x = rcTiles.left + x * 8;
	pt.y = rcTiles.top + y * 8;
	VDPSelectTile_KMod(pt);
	return TRUE;
}

static LRESULT CALLBACK VDPTileWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_GETDLGCODE)
		return CallWindowProc(tileWindowProc, hwnd, message, wParam, lParam) | DLGC_WANTARROWS;
	if (message == WM_KEYDOWN && VDPNavigateTile_KMod(wParam)) return 0;
	return CallWindowProc(tileWindowProc, hwnd, message, wParam, lParam);
}

void VDPInit_KMod(HWND hwnd)
{
	RECT TZoomRect, windowRect, clientRect;

	GetClientRect(GetDlgItem(hwnd, IDC_VDP_PREVIEW), &TZoomRect);

	zoomTile_KMod = (UCHAR)((TZoomRect.right - TZoomRect.left) / 8);
	zoomTile_KMod = (UCHAR)min(zoomTile_KMod, (TZoomRect.bottom - TZoomRect.top) / 8);

	pal_KMod = 0;

	hWndPal = GetDlgItem(hwnd, IDC_VDP_PAL);
	GetClientRect(hWndPal, &rcPal);
	palV = (rcPal.bottom - rcPal.top) / VDP_PALETTES;//4;
	palH = (rcPal.right - rcPal.left) / 16;

	hWndRedArrow = GetDlgItem(hwnd, IDC_VDP_PAL_ARROW);
	GetClientRect(hWndRedArrow, &rcArrow);

	MapWindowPoints(hWndPal, hwnd, (LPPOINT)&rcPal, 2);

	GetClientRect(GetDlgItem(hwnd, IDC_VDP_TILES), &rcTiles);
	MapWindowPoints(GetDlgItem(hwnd, IDC_VDP_TILES), hwnd, (LPPOINT)&rcTiles, 2);

	rcTiles.bottom = rcTiles.top + VDP_TILE_ROWS * 8;
	rcTiles.right = rcTiles.left + VDP_TILE_COLUMNS * 8;
	MoveWindow(GetDlgItem(hwnd, IDC_VDP_TILES), rcTiles.left, rcTiles.top, rcTiles.right - rcTiles.left, rcTiles.bottom - rcTiles.top, TRUE);

	MoveWindow(GetDlgItem(hwnd, IDC_VDP_TILE_ZOOM_ADR), rcTiles.left, rcTiles.bottom + 8, TZoomRect.right - TZoomRect.left, 16, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDC_VDP_PREVIEW), rcTiles.left, rcTiles.bottom + 8 + 16 + 4, TZoomRect.right - TZoomRect.left, TZoomRect.bottom - TZoomRect.top, TRUE);

	CheckDlgButton(hwnd, IDC_VDP_PAL_1, BST_CHECKED);
	CheckDlgButton(hwnd, IDC_VDP_PAL_2, BST_CHECKED);
	CheckDlgButton(hwnd, IDC_VDP_PAL_3, BST_CHECKED);
	CheckDlgButton(hwnd, IDC_VDP_PAL_4, BST_CHECKED);
	ActivePal = 0x0F;

	/* Include all VRAM tiles and retain a margin at the right edge. */
	GetWindowRect(hwnd, &windowRect);
	GetClientRect(hwnd, &clientRect);
	SetWindowPos(hwnd, NULL, 0, 0,
		max(clientRect.right, rcTiles.right + rcTiles.left) +
			windowRect.right - windowRect.left - clientRect.right,
		windowRect.bottom - windowRect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

}


BOOL CALLBACK VDPDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	switch (Message)
	{
	case WM_INITDIALOG:
		tileWindowProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwnd, IDC_VDP_TILES),
			GWLP_WNDPROC, (LONG_PTR)VDPTileWindowProc);
		VDPInit_KMod(hwnd);
		{
			RECT rect;
			GetWindowRect(hwnd, &rect);
			minimumVDPSize.cx = rect.right - rect.left;
			minimumVDPSize.cy = rect.bottom - rect.top;
		}
		break;

	case WM_KEYDOWN:
		return VDPNavigateTile_KMod(wParam);

	case WM_GETMINMAXINFO:
		if (minimumVDPSize.cx && minimumVDPSize.cy)
		{
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = minimumVDPSize.cx;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = minimumVDPSize.cy;
		}
		break;

	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_VDP_PAL)
			DrawVDP_Pal_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == IDC_VDP_TILES)
			DrawVDP_Tiles_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == IDC_VDP_PREVIEW)
			DrawVDPZoom_Tiles_KMod((LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_VDP_PAL:
			VDPPal_Choose();
			break;
		case IDC_VDP_TILES:
			VDPTile_Choose();
			break;
		case IDC_VDP_VRAM:
			DumpVRAM_KMod(hwnd);
			break;
		case IDC_VDP_CRAM:
			DumpCRAM_KMod(hwnd);
			break;
		case IDC_VDP_BMP:
			DumpTiles_KMod(hwnd);
			break;
		case IDC_VDP_PAL_1:
			if (IsDlgButtonChecked(hwnd, IDC_VDP_PAL_1) == BST_CHECKED)
				ActivePal |= 0x01;
			else
				ActivePal &= 0x0E;
			CRam_Flag = 1;
			break;
		case IDC_VDP_PAL_2:
			if (IsDlgButtonChecked(hwnd, IDC_VDP_PAL_2) == BST_CHECKED)
				ActivePal |= 0x02;
			else
				ActivePal &= 0x0D;
			CRam_Flag = 1;
			break;
		case IDC_VDP_PAL_3:
			if (IsDlgButtonChecked(hwnd, IDC_VDP_PAL_3) == BST_CHECKED)
				ActivePal |= 0x04;
			else
				ActivePal &= 0x0B;
			CRam_Flag = 1;
			break;
		case IDC_VDP_PAL_4:
			if (IsDlgButtonChecked(hwnd, IDC_VDP_PAL_4) == BST_CHECKED)
				ActivePal |= 0x08;
			else
				ActivePal &= 0x07;
			CRam_Flag = 1;
			break;
		}
		break;

	case WM_CLOSE:
		CloseWindow_KMod(DMODE_VDP);
		break;

	case WM_DESTROY:
		hVDP = NULL;
		OpenedWindow_KMod[DMODE_VDP - 1] = FALSE;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void vdpdebug_create(HINSTANCE hInstance, HWND hWndParent)
{
	hVDP = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGVDP), hWndParent, VDPDlgProc);
}

void vdpdebug_show(BOOL visibility)
{
	ShowWindow(hVDP, visibility ? SW_SHOW : SW_HIDE);
}

void vdpdebug_update()
{
	if (OpenedWindow_KMod[DMODE_VDP-1] == FALSE)	return;

	UpdateVDP_KMod();
}

void vdpdebug_reset()
{

}
void vdpdebug_destroy()
{
	DestroyWindow(hVDP);
}


void vdpdebug_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "VDPOpen",
		OpenedWindow_KMod[DMODE_VDP - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(hVDP, "VDPRect", config_file);
}

void vdpdebug_restore_window(const char *config_file)
{
	BOOL visible = GetPrivateProfileInt("DebugWindows", "VDPOpen", 0, config_file) != 0;
	DebugWindow_RestoreGeometry(hVDP, "VDPRect", config_file);
	{
		RECT rect;
		GetWindowRect(hVDP, &rect);
		SetWindowPos(hVDP, NULL, 0, 0,
			max(rect.right - rect.left, minimumVDPSize.cx),
			max(rect.bottom - rect.top, minimumVDPSize.cy),
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	OpenedWindow_KMod[DMODE_VDP - 1] = visible && hVDP != NULL;
	vdpdebug_show(visible);
}


BOOL vdpdebug_isMessage(LPMSG lpMsg)
{
		if (IsDialogMessage(hVDP, lpMsg))
		{
			TranslateAccelerator(hVDP, hAccelTable, lpMsg);
			return TRUE;
		}
	

	return FALSE;
}


void vdpdebug_drawTile(HDC hDCMain, unsigned short int numTile, WORD x, WORD y, UCHAR pal, UCHAR zoom)
{
	int j;
	COLORREF tile_data[8 * 8];
	COLORREF *ptr = &tile_data[0];
	unsigned int tile_row;

	static const struct BITMAPINFO_LOCAL
	{
		BITMAPINFOHEADER hdr;
		COLORREF colors[3];
	}
	bmi =
	{
		{
			sizeof(BITMAPINFOHEADER),   // biSize
			8,                          // biWidth
			-8,                         // biHeight
			1,                          // biPlanes
			32,                         // biBitCount
			BI_BITFIELDS,               // biCompression
			0,                          // biSizeImage
			75,                         // biXPelsPerMeter
			75,                         // biYPelsPerMeter
			0,                          // biClrUsed
			0                           // biClrImportant
		},
		{
			0x000000FF,
			0x0000FF00,
			0x00FF0000,
		}
	};
	const unsigned int * __restrict vram_dwords = (const unsigned int *)VRam;

	for (j = 0; j < 8; j++)
	{
		tile_row = vram_dwords[numTile * 8 + j];
		ptr[3] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[2] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[1] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[0] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[7] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[6] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[5] = vdpdebug_getColor(pal, tile_row & 0xF);
		tile_row >>= 4;
		ptr[4] = vdpdebug_getColor(pal, tile_row & 0xF);
		ptr += 8;
	}

	StretchDIBits(
		hDCMain,
		x, y,
		8 * zoom, 8 * zoom,
		0, 0,
		8, 8,
		tile_data,
		(const BITMAPINFO *)&bmi,
		DIB_RGB_COLORS,
		SRCCOPY);
}

COLORREF vdpdebug_getColor(unsigned int numPal, unsigned int numColor)
{
	/* can't use MD_Palette since it's DirectX mode (555 or 565) */
	/* !! CRAM is (binary:)GGG0RRR00000BBB0   while COLORREF is (hexa:)0x00BBGGRR */
	unsigned short int md_color;
	COLORREF newColor;

	// handle our false CGA pal
	if (numPal == VDP_PALETTE_CGA)	return (cgaPal[numColor]);
	if (numPal == VDP_PALETTE_MONO)	return (monoPal[numColor]);

	/* B */
	newColor = (COLORREF)CRam[2 * 16 * numPal + 2 * numColor + 1];
	newColor <<= 20;

	/* G */
	md_color = (unsigned short int) (CRam[2 * 16 * numPal + 2 * numColor] & 0xF0);
	md_color <<= 8;
	newColor |= md_color;

	/* R */
	md_color = (unsigned short int) (CRam[2 * 16 * numPal + 2 * numColor] & 0x0F);
	md_color <<= 4;
	newColor |= md_color;

	//transform genny white to true white
	if (newColor == 0x00E0E0E0)
		newColor = 0x00FFFFFF;

	return newColor;
}