#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../G_main.h" //for Paused
#include "../resource.h"
#include "../Mem_S68k.h"
#include "../G_gfx.h" //used for Put_Info

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "cdgfx.h"
#include "window_geometry.h"
//// size WRAM = 256*1024....4096tiles (64b) ==> (32*8)*(64*16)

#include "vdp.h"
//TODO what the exact use of pal_Kmod here ? why not CDpal_KMod ?

static HWND hCD_GFX;
// 4096 8x16 tiles cover all 256 KiB of Word RAM.
#define CD_GFX_COLUMNS 128
#define CD_GFX_ROWS 32
#define CD_GFX_WIDTH (CD_GFX_COLUMNS * 8)
#define CD_GFX_HEIGHT (CD_GFX_ROWS * 16)
#define CD_GFX_PREVIEW_ID 50990
#define CD_GFX_PREVIEW_WIDTH 64
#define CD_GFX_PREVIEW_HEIGHT 128

static unsigned int selectedTile;
static WNDPROC imageProc;
static int displayedWordRamState = -1;

static void CD_GFXUpdateMode(HWND hwnd)
{
	/* States 0/1 are 2M; states 2/3 are the two 1M bank assignments. */
	int state = Ram_Word_State & 3;
	if (state == displayedWordRamState) return;
	if (state & 2)
		SetWindowText(hwnd, (state & 1)
			? "CD Word RAM (1M mode) - Bank 0: Mega CD / Bank 1: Mega Drive"
			: "CD Word RAM (1M mode) - Bank 0: Mega Drive / Bank 1: Mega CD");
	else
		SetWindowText(hwnd, "CD Word RAM (2M mode)");
	displayedWordRamState = state;
}

static unsigned char CD_GFXPixel(unsigned int tile, unsigned int x, unsigned int y)
{
	unsigned char data = Ram_Word_2M[tile * 64 + y * 4 + ((x / 2) ^ 1)];
	return (x & 1) ? (data & 15) : (data >> 4);
}

static void CD_GFXSetSelectedTile(HWND hwnd, unsigned int tile)
{
	char address[32];
	if (tile >= CD_GFX_COLUMNS * CD_GFX_ROWS) return;
	selectedTile = tile;
	wsprintf(address, "0x%06X", selectedTile * 64);
	SetDlgItemText(hwnd, IDC_CD_GFX_ADR, address);
	InvalidateRect(GetDlgItem(hwnd, IDC_CD_GFX), NULL, FALSE);
	InvalidateRect(GetDlgItem(hwnd, CD_GFX_PREVIEW_ID), NULL, FALSE);
}

static BOOL CD_GFXNavigate(HWND hwnd, WPARAM key)
{
	if (key == VK_LEFT)
	{
		if (selectedTile > 0) CD_GFXSetSelectedTile(hwnd, selectedTile - 1);
		return TRUE;
	}
	if (key == VK_RIGHT)
	{
		CD_GFXSetSelectedTile(hwnd, selectedTile + 1);
		return TRUE;
	}
	if (key == VK_UP)
	{
		if (selectedTile >= CD_GFX_COLUMNS)
			CD_GFXSetSelectedTile(hwnd, selectedTile - CD_GFX_COLUMNS);
		return TRUE;
	}
	if (key == VK_DOWN)
	{
		CD_GFXSetSelectedTile(hwnd, selectedTile + CD_GFX_COLUMNS);
		return TRUE;
	}
	return FALSE;
}

static void CD_GFXSelectTile(HWND hwnd, int x, int y)
{
	RECT rect;
	GetClientRect(GetDlgItem(hwnd, IDC_CD_GFX), &rect);
	if (x < 0 || y < 0 || x >= rect.right || y >= rect.bottom) return;
	CD_GFXSetSelectedTile(hwnd, (y * CD_GFX_HEIGHT / rect.bottom / 16) * CD_GFX_COLUMNS +
		x * CD_GFX_WIDTH / rect.right / 8);
}

static LRESULT CALLBACK CD_GFXImageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_LBUTTONDOWN)
	{
		SetFocus(hwnd);
		CD_GFXSelectTile(GetParent(hwnd), (short)LOWORD(lParam), (short)HIWORD(lParam));
		return 0;
	}
	if (message == WM_GETDLGCODE)
		return CallWindowProc(imageProc, hwnd, message, wParam, lParam) | DLGC_WANTARROWS;
	if (message == WM_KEYDOWN && CD_GFXNavigate(GetParent(hwnd), wParam)) return 0;
	return CallWindowProc(imageProc, hwnd, message, wParam, lParam);
}

static void CD_GFXDrawPreview(LPDRAWITEMSTRUCT draw)
{
	DWORD pixels[8 * 16];
	BITMAPINFO bmi;
	unsigned int x, y;
	for (y = 0; y < 16; y++)
		for (x = 0; x < 8; x++)
		{
			COLORREF color = vdpdebug_getColor(pal_KMod, CD_GFXPixel(selectedTile, x, y));
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





void DumpTilesCD_KMod(HWND hwnd)
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
	WORD maxX, maxY;

	COLORREF		tmpColor;
	DWORD			j;

	unsigned short int numTile = 0;
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
	szFile.lpstrTitle = "Convert Word RAM to bitmap";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "bmp";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	bmfh.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"
	bmfh.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD) + 64 * 2000 / 2); // 4bits per pixel
	bmfh.bfOffBits = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD));


	maxY = 64 * 16;
	maxX = 32 * 8;

	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth = maxX;
	bmiHeader.biHeight = maxY;
	bmiHeader.biPlanes = 0;
	bmiHeader.biBitCount = 4;
	bmiHeader.biClrUsed = 16;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = (bmiHeader.biWidth*bmiHeader.biHeight) / 2;
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

	for (posY = 0; posY < maxY; posY += 16)
	{
		for (posX = 0; posX < maxX / 2; posX += 4)
		{
			for (j = 0; j < 8; j++)
			{
				TileData = Ram_Word_2M[numTile * 64 + j * 4 + 1];
				pBits[(bmiHeader.biHeight - (posY + j))*bmiHeader.biWidth / 2 + posX + 0] = TileData;

				TileData = Ram_Word_2M[numTile * 64 + j * 4];
				pBits[(bmiHeader.biHeight - (posY + j))*bmiHeader.biWidth / 2 + posX + 1] = TileData;

				TileData = Ram_Word_2M[numTile * 64 + j * 4 + 3];
				pBits[(bmiHeader.biHeight - (posY + j))*bmiHeader.biWidth / 2 + posX + 2] = TileData;

				TileData = Ram_Word_2M[numTile * 64 + j * 4 + 2];
				pBits[(bmiHeader.biHeight - (posY + j))*bmiHeader.biWidth / 2 + posX + 3] = TileData;
			}

			for (j = 0; j < 8; j++)
			{
				TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 1];
				pBits[(bmiHeader.biHeight - (posY + j + 8))*bmiHeader.biWidth / 2 + posX + 0] = TileData;

				TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4];
				pBits[(bmiHeader.biHeight - (posY + j + 8))*bmiHeader.biWidth / 2 + posX + 1] = TileData;

				TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 3];
				pBits[(bmiHeader.biHeight - (posY + j + 8))*bmiHeader.biWidth / 2 + posX + 2] = TileData;

				TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 2];
				pBits[(bmiHeader.biHeight - (posY + j + 8))*bmiHeader.biWidth / 2 + posX + 3] = TileData;
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

	Put_Info("WRAM Tiles dumped", 1500);
}


void DrawTileCD_KMod(HDC hDCMain, unsigned short int numTile, WORD x, WORD y, UCHAR pal, UCHAR zoom)
{
	unsigned char j;
	unsigned char TileData;
	HDC hDC;
	HBITMAP hBitmap, hOldBitmap;

	hDC = CreateCompatibleDC(hDCMain);
	hBitmap = CreateCompatibleBitmap(hDCMain, 8, 16);
	hOldBitmap = SelectObject(hDC, hBitmap);


	for (j = 0; j < 8; j++)
	{
		TileData = Ram_Word_2M[numTile * 64 + j * 4 + 1] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 0, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4 + 1] & 0x0F;
		SetPixelV(hDC, 1, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 2, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4] & 0x0F;
		SetPixelV(hDC, 3, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4 + 3] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 4, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4 + 3] & 0x0F;
		SetPixelV(hDC, 5, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4 + 2] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 6, j, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + j * 4 + 2] & 0x0F;
		SetPixelV(hDC, 7, j, vdpdebug_getColor(pal, TileData));
	}

	for (j = 0; j < 8; j++)
	{
		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 1] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 0, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 1] & 0x0F;
		SetPixelV(hDC, 1, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 2, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4] & 0x0F;
		SetPixelV(hDC, 3, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 3] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 4, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 3] & 0x0F;
		SetPixelV(hDC, 5, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 2] & 0xF0;
		TileData >>= 4;
		SetPixelV(hDC, 6, j + 8, vdpdebug_getColor(pal, TileData));

		TileData = Ram_Word_2M[numTile * 64 + 32 + j * 4 + 2] & 0x0F;
		SetPixelV(hDC, 7, j + 8, vdpdebug_getColor(pal, TileData));
	}

	StretchBlt(
		hDCMain, // handle to destination device context
		x,  // x-coordinate of destination rectangle's upper-left
		// corner
		y,  // y-coordinate of destination rectangle's upper-left
		// corner
		8 * zoom,  // width of destination rectangle
		16 * zoom, // height of destination rectangle
		hDC,  // handle to source device context
		0,   // x-coordinate of source rectangle's upper-left
		// corner
		0,   // y-coordinate of source rectangle's upper-left
		// corner
		8,
		16,
		SRCCOPY  // raster operation code
		);


	SelectObject(hDC, hOldBitmap);
	DeleteObject(hBitmap);

	DeleteDC(hDC);
}


void DrawCD_GFX_KMod(LPDRAWITEMSTRUCT hlDIS)
{
	static DWORD pixels[CD_GFX_WIDTH * CD_GFX_HEIGHT];
	DWORD colors[16];
	BITMAPINFO bmi;
	unsigned int tile, x, y, i;

	for (i = 0; i < 16; ++i)
	{
		COLORREF color = vdpdebug_getColor(pal_KMod, i);
		colors[i] = (GetRValue(color) << 16) | (GetGValue(color) << 8) | GetBValue(color);
	}

	// Write pixels directly instead of allocating a GDI bitmap for every tile.
	for (tile = 0; tile < CD_GFX_COLUMNS * CD_GFX_ROWS; ++tile)
	{
		unsigned int tileX = (tile % CD_GFX_COLUMNS) * 8;
		unsigned int tileY = (tile / CD_GFX_COLUMNS) * 16;
		for (y = 0; y < 16; ++y)
		{
			for (x = 0; x < 8; x += 2)
			{
				unsigned char data = Ram_Word_2M[tile * 64 + y * 4 + ((x / 2) ^ 1)];
				pixels[(tileY + y) * CD_GFX_WIDTH + tileX + x] = colors[data >> 4];
				pixels[(tileY + y) * CD_GFX_WIDTH + tileX + x + 1] = colors[data & 15];
			}
		}
	}

	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = CD_GFX_WIDTH;
	bmi.bmiHeader.biHeight = -CD_GFX_HEIGHT;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	SetStretchBltMode(hlDIS->hDC, COLORONCOLOR);
	StretchDIBits(hlDIS->hDC, hlDIS->rcItem.left, hlDIS->rcItem.top,
		hlDIS->rcItem.right - hlDIS->rcItem.left, hlDIS->rcItem.bottom - hlDIS->rcItem.top,
		0, 0, CD_GFX_WIDTH, CD_GFX_HEIGHT, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
	{
		RECT selection;
		int width = hlDIS->rcItem.right - hlDIS->rcItem.left;
		int height = hlDIS->rcItem.bottom - hlDIS->rcItem.top;
		int x = selectedTile % CD_GFX_COLUMNS;
		int y = selectedTile / CD_GFX_COLUMNS;
		selection.left = hlDIS->rcItem.left + MulDiv(x * 8, width, CD_GFX_WIDTH);
		selection.top = hlDIS->rcItem.top + MulDiv(y * 16, height, CD_GFX_HEIGHT);
		selection.right = hlDIS->rcItem.left + MulDiv((x + 1) * 8, width, CD_GFX_WIDTH);
		selection.bottom = hlDIS->rcItem.top + MulDiv((y + 1) * 16, height, CD_GFX_HEIGHT);
		FrameRect(hlDIS->hDC, &selection, (HBRUSH)GetStockObject(WHITE_BRUSH));
	}
}


void UpdateCD_GFX_KMod()
{
	if (OpenedWindow_KMod[DMODE_CD_GFX - 1] == FALSE) return;
	CD_GFXUpdateMode(hCD_GFX);
	RedrawWindow(GetDlgItem(hCD_GFX, IDC_CD_GFX), NULL, NULL, RDW_INVALIDATE);
	InvalidateRect(GetDlgItem(hCD_GFX, CD_GFX_PREVIEW_ID), NULL, FALSE);
}


void CD_GFXInit_KMod(HWND hwnd)
{
	RECT image, frame = {0, 0, 0, 0}, window;
	MONITORINFO monitor;
	int width = CD_GFX_WIDTH, height = CD_GFX_HEIGHT;
	int margin, clientWidth, clientHeight;

	CD_GFXUpdateMode(hwnd);

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
			(frame.bottom - frame.top) - image.top - margin * 2 - CD_GFX_PREVIEW_HEIGHT;
		if (availableWidth < width && availableWidth > 0)
		{
			width = availableWidth;
			height = MulDiv(width, CD_GFX_HEIGHT, CD_GFX_WIDTH);
		}
		if (availableHeight < height && availableHeight > 0)
		{
			height = availableHeight;
			width = MulDiv(height, CD_GFX_WIDTH, CD_GFX_HEIGHT);
		}
	}

	clientWidth = width + margin * 2;
	clientHeight = image.top + height + margin * 2 + CD_GFX_PREVIEW_HEIGHT;
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX), margin, image.top, width, height, TRUE);
	MoveWindow(GetDlgItem(hwnd, CD_GFX_PREVIEW_ID), margin, image.top + height + margin,
		CD_GFX_PREVIEW_WIDTH, CD_GFX_PREVIEW_HEIGHT, TRUE);
	/* Give the address the remaining width, including room for A-F digits. */
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX_ADR), margin * 2 + CD_GFX_PREVIEW_WIDTH,
		image.top + height + margin, max(0, clientWidth - margin * 3 - CD_GFX_PREVIEW_WIDTH),
		CD_GFX_PREVIEW_HEIGHT, TRUE);
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


BOOL CALLBACK CD_GFXDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_INITDIALOG:
		selectedTile = 0;
		displayedWordRamState = -1;
		HandleWindow_KMod[DMODE_CD_GFX - 1] = hwnd;
		CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
			0, 0, 0, 0, hwnd, (HMENU)CD_GFX_PREVIEW_ID, GetModuleHandle(NULL), NULL);
		imageProc = (WNDPROC)SetWindowLongPtr(GetDlgItem(hwnd, IDC_CD_GFX), GWLP_WNDPROC,
			(LONG_PTR)CD_GFXImageProc);
		CD_GFXInit_KMod(hwnd);
		break;

	case WM_KEYDOWN:
		return CD_GFXNavigate(hwnd, wParam);

	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_CD_GFX)
			DrawCD_GFX_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == CD_GFX_PREVIEW_ID)
			CD_GFXDrawPreview((LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) != IDCANCEL) return FALSE;
	case WM_CLOSE:
		CloseWindow_KMod(DMODE_CD_GFX);
		break;

	case WM_DESTROY:
		hCD_GFX = NULL;
		HandleWindow_KMod[DMODE_CD_GFX - 1] = NULL;
		OpenedWindow_KMod[DMODE_CD_GFX - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

void cdgfx_create(HINSTANCE hInstance, HWND hWndParent)
{
	hCD_GFX = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGCD_GFX), hWndParent, CD_GFXDlgProc);
}

void cdgfx_show(BOOL visibility)
{
	if (visibility) CD_GFXUpdateMode(hCD_GFX);
	ShowWindow(hCD_GFX, visibility ? SW_SHOW : SW_HIDE);
}

void cdgfx_update()
{
	if (OpenedWindow_KMod[DMODE_CD_GFX-1] == FALSE)	return;

	UpdateCD_GFX_KMod();
}

void cdgfx_reset()
{
	CD_GFXInit_KMod(hCD_GFX);
}
void cdgfx_destroy()
{
	if (hCD_GFX) DestroyWindow(hCD_GFX);
}

void cdgfx_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "CDGfxOpen",
		OpenedWindow_KMod[DMODE_CD_GFX - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(hCD_GFX, "CDGfxRect", config_file);
}

void cdgfx_restore_window(const char *config_file)
{
	RECT rect;
	BOOL visible = GetPrivateProfileInt("DebugWindows", "CDGfxOpen", 0, config_file) != 0;
	if (!hCD_GFX) return;
	GetWindowRect(hCD_GFX, &rect);
	DebugWindow_RestoreGeometry(hCD_GFX, "CDGfxRect", config_file);
	/* Keep the image layout calculated for the current display. */
	SetWindowPos(hCD_GFX, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
		SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	OpenedWindow_KMod[DMODE_CD_GFX - 1] = visible;
	cdgfx_show(visible);
}

