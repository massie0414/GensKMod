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
//// size WRAM = 256*1024....4096tiles (64b) ==> (32*8)*(64*16)

#include "vdp.h"
//TODO what the exact use of pal_Kmod here ? why not CDpal_KMod ?

static HWND hCD_GFX;
// 4096 8x16 tiles cover all 256 KiB of Word RAM.
#define CD_GFX_COLUMNS 128
#define CD_GFX_ROWS 32
#define CD_GFX_WIDTH (CD_GFX_COLUMNS * 8)
#define CD_GFX_HEIGHT (CD_GFX_ROWS * 16)





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
}


void UpdateCD_GFX_KMod()
{
	if (OpenedWindow_KMod[5] == FALSE) return;
	RedrawWindow(GetDlgItem(hCD_GFX, IDC_CD_GFX), NULL, NULL, RDW_INVALIDATE);
}


void CD_GFXInit_KMod(HWND hwnd)
{
	RECT image, button, frame = {0, 0, 0, 0}, window;
	MONITORINFO monitor;
	int width = CD_GFX_WIDTH, height = CD_GFX_HEIGHT;
	int margin, buttonHeight, buttonWidth, clientWidth, clientHeight;

	GetWindowRect(GetDlgItem(hwnd, IDC_CD_GFX), &image);
	MapWindowPoints(NULL, hwnd, (LPPOINT)&image, 2);
	GetWindowRect(GetDlgItem(hwnd, IDC_CD_GFX_BMP), &button);
	margin = image.left;
	buttonHeight = button.bottom - button.top;
	buttonWidth = button.right - button.left;
	AdjustWindowRectEx(&frame, GetWindowLong(hwnd, GWL_STYLE), FALSE,
		GetWindowLong(hwnd, GWL_EXSTYLE));
	monitor.cbSize = sizeof(monitor);
	if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
	{
		int availableWidth = monitor.rcWork.right - monitor.rcWork.left -
			(frame.right - frame.left) - margin * 2;
		int availableHeight = monitor.rcWork.bottom - monitor.rcWork.top -
			(frame.bottom - frame.top) - image.top - buttonHeight - margin * 2;
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
	clientHeight = image.top + height + margin * 2 + buttonHeight;
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX), margin, image.top, width, height, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX_DUMP), margin, image.top + height + margin,
		buttonWidth, buttonHeight, TRUE);
	MoveWindow(GetDlgItem(hwnd, IDC_CD_GFX_BMP), clientWidth - margin - buttonWidth,
		image.top + height + margin, buttonWidth, buttonHeight, TRUE);
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
		CD_GFXInit_KMod(hwnd);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CD_GFX_DUMP:
			//TODO
			break;

		case IDC_CD_GFX_BMP:
			DumpTilesCD_KMod(hCD_GFX);
			break;
		}
		break;

	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_CD_GFX)
			DrawCD_GFX_KMod((LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_CLOSE:
		CloseWindow_KMod(DMODE_CD_GFX);
		break;

	case WM_DESTROY:
		cdgfx_destroy();
		PostQuitMessage(0);
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
	DestroyWindow(hCD_GFX);
}

