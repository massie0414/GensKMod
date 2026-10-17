#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../vdp_io.h"
#include "../G_gfx.h" //used for Put_Info


//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "sprites.h"
#include "planes.h"
#include "window_geometry.h"
#include "vdp.h"


static HWND hSprites;
static SIZE minimumSpritesSize;
static HWND hSpriteList;
static HWND hSpriteList2;
static HWND hSpriteGallery;
#define SPRITE_COLUMNS 8
#define SPRITE_ROWS 10
#define SPRITE_CELL_WIDTH 48
#define SPRITE_CELL_HEIGHT 52

static int SelectedSprite_KMod(void)
{
	int index = ListView_GetNextItem(hSpriteList, -1, LVNI_FOCUSED);
	if (index >= 0) return index;
	index = ListView_GetNextItem(hSpriteList2, -1, LVNI_FOCUSED);
	return index < 0 ? -1 : index + 40;
}

static void SelectSpriteGallery_KMod(POINT pt)
{
	int index;
	if (pt.x < 0 || pt.y < 0 || pt.x >= SPRITE_COLUMNS * SPRITE_CELL_WIDTH ||
		pt.y >= SPRITE_ROWS * SPRITE_CELL_HEIGHT) return;
	index = (pt.y / SPRITE_CELL_HEIGHT) * SPRITE_COLUMNS + pt.x / SPRITE_CELL_WIDTH;
	ListView_SetItemState(hSpriteList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	ListView_SetItemState(hSpriteList2, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	ListView_SetItemState(index < 40 ? hSpriteList : hSpriteList2, index % 40, LVIS_SELECTED | LVIS_FOCUSED,
		LVIS_SELECTED | LVIS_FOCUSED);
	ListView_EnsureVisible(index < 40 ? hSpriteList : hSpriteList2, index % 40, FALSE);
}

static LRESULT CALLBACK SpriteListKeyProc_KMod(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR subclassId, DWORD_PTR reference)
{
	if (message == WM_KEYDOWN)
	{
		int row = ListView_GetNextItem(hwnd, -1, LVNI_FOCUSED);
		HWND target = NULL;
		int targetRow = 0;
		if (hwnd == hSpriteList && row == 39 && wParam == VK_DOWN)
			target = hSpriteList2;
		else if (hwnd == hSpriteList2 && row == 0 && wParam == VK_UP)
		{
			target = hSpriteList;
			targetRow = 39;
		}
		if (target)
		{
			ListView_SetItemState(hwnd, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_SetItemState(target, targetRow, LVIS_SELECTED | LVIS_FOCUSED,
				LVIS_SELECTED | LVIS_FOCUSED);
			ListView_EnsureVisible(target, targetRow, FALSE);
			SetFocus(target);
			return 0;
		}
	}
	if (message == WM_NCDESTROY) RemoveWindowSubclass(hwnd, SpriteListKeyProc_KMod, subclassId);
	return DefSubclassProc(hwnd, message, wParam, lParam);
}

unsigned char TrueSize_KMod(unsigned short int data);

static void DrawSpriteGallery_KMod(LPDRAWITEMSTRUCT draw)
{
	HDC dc = CreateCompatibleDC(draw->hDC), spriteDC = CreateCompatibleDC(draw->hDC);
	HBITMAP bitmap = CreateCompatibleBitmap(draw->hDC, SPRITE_COLUMNS * SPRITE_CELL_WIDTH,
		SPRITE_ROWS * SPRITE_CELL_HEIGHT);
	HBITMAP sprite = CreateCompatibleBitmap(draw->hDC, 32, 32);
	HBITMAP oldBitmap = SelectObject(dc, bitmap), oldSprite = SelectObject(spriteDC, sprite);
	HFONT oldFont = SelectObject(dc, (HFONT)SendMessage(hSpriteList, WM_GETFONT, 0, 0));
	int i, x, y, selected = SelectedSprite_KMod();
	RECT spriteRect = {0, 0, 32, 32};
	SetBkMode(dc, TRANSPARENT);
	FillRect(dc, &draw->rcItem, GetSysColorBrush(COLOR_3DFACE));
	for (i = 0; i < 80; i++)
	{
		unsigned short *data = (unsigned short *)(VRam + (VDP_Reg.Spr_Att_Adr << 9)) + i * 4;
		int width = TrueSize_KMod((data[1] >> 10) & 3);
		int height = TrueSize_KMod((data[1] >> 8) & 3);
		unsigned short tile = data[2] & 0x7FF;
		int left = (i % SPRITE_COLUMNS) * SPRITE_CELL_WIDTH;
		int top = (i / SPRITE_COLUMNS) * SPRITE_CELL_HEIGHT;
		RECT cell = {left, top, left + SPRITE_CELL_WIDTH, top + SPRITE_CELL_HEIGHT};
		RECT label = {left, top, left + SPRITE_CELL_WIDTH, top + 18};
		char number[8];
		/* An empty attribute entry does not represent an assigned sprite. */
		if (data[0] || data[1] || data[2] || data[3])
		{
			FillRect(spriteDC, &spriteRect, GetSysColorBrush(COLOR_3DFACE));
			for (x = 0; x < width; x += 8)
				for (y = 0; y < height; y += 8)
					vdpdebug_drawTile(spriteDC, tile++ & 0x7FF, (WORD)x, (WORD)y,
						(UCHAR)((data[2] >> 13) & 3), 1);
			StretchBlt(dc, left + (SPRITE_CELL_WIDTH - width) / 2, top + 18, width, height,
				spriteDC, data[2] & 0x0800 ? width - 1 : 0, data[2] & 0x1000 ? height - 1 : 0,
				data[2] & 0x0800 ? -width : width, data[2] & 0x1000 ? -height : height, SRCCOPY);
		}
		wsprintf(number, "%02d", i);
		SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
		DrawText(dc, number, -1, &label, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
		if (i == selected) FrameRect(dc, &cell, GetSysColorBrush(COLOR_HIGHLIGHT));
	}
	BitBlt(draw->hDC, 0, 0, SPRITE_COLUMNS * SPRITE_CELL_WIDTH,
		SPRITE_ROWS * SPRITE_CELL_HEIGHT, dc, 0, 0, SRCCOPY);
	SelectObject(dc, oldFont);
	SelectObject(dc, oldBitmap);
	SelectObject(spriteDC, oldSprite);
	DeleteObject(bitmap);
	DeleteObject(sprite);
	DeleteDC(dc);
	DeleteDC(spriteDC);
}


unsigned char TrueSize_KMod(unsigned short int data)
{
	switch (data)
	{
	case 0:
		return 8;
	case 1:
		return 16;
	case 2:
		return 24;
	case 3:
		return 32;
	}

	return 0;
}


void SpritesInit_KMod(HWND hDlg)
{
	LV_COLUMN   lvColumn;
	int         i, group;
	TCHAR       szString[8][7] = { "Num", "Ypos", "XPos", "Size", "Link", "Pal", "Tile", "Flags*" };
	LVITEM		lvItem;
	char		buf[64];


	hSpriteList = GetDlgItem(hDlg, IDC_SPRITES_LIST);
	if (!hSpriteList2)
	{
		hSpriteList2 = CreateWindow(WC_LISTVIEW, "", GetWindowLong(hSpriteList, GWL_STYLE),
			0, 0, 1, 1, hDlg, (HMENU)IDC_SPRITES_LIST2,
			(HINSTANCE)GetWindowLongPtr(hDlg, GWLP_HINSTANCE), NULL);
		SendMessage(hSpriteList2, WM_SETFONT, SendMessage(hSpriteList, WM_GETFONT, 0, 0), FALSE);
	}
	for (group = 0; group < 2; group++)
	{
		HWND list = group ? hSpriteList2 : hSpriteList;
		SetWindowSubclass(list, SpriteListKeyProc_KMod, 1, 0);
		ListView_DeleteAllItems(list);
		lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT;
		lvColumn.fmt = LVCFMT_LEFT;
		lvColumn.cx = 53;
		for (i = 0; i < 8; i++)
		{
			lvColumn.pszText = szString[i];
			if (Header_GetItemCount(ListView_GetHeader(list)) <= i)
				ListView_InsertColumn(list, i, &lvColumn);
		}
		ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		for (i = 0; i < 40; i++)
		{
			wsprintf(buf, "%0.2d", group * 40 + i);
			lvItem.mask = LVIF_TEXT;
			lvItem.iItem = i;
			lvItem.iSubItem = 0;
			lvItem.pszText = buf;
			ListView_InsertItem(list, &lvItem);
		}
	}

}

void UpdateSprites_KMod()
{
	unsigned int i;
	unsigned short int *sprData, tmp;
	unsigned char tmp_string[32];
	static unsigned short data_copy[80 * 4];
	static unsigned int forceRefreshCounter;

	/* Patterns and palette can animate without changes to the attribute table. */
	if (hSpriteGallery) RedrawWindow(hSpriteGallery, NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(GetDlgItem(hSprites, IDC_SPRITES_PREVIEW2), NULL, NULL, RDW_INVALIDATE);
	sprData = (unsigned short *)(VRam + (VDP_Reg.Spr_Att_Adr << 9));


	if (!memcmp(sprData, data_copy, sizeof(data_copy)))
	{
		//sometimes miss the last update... so force refresh every n frame
		if (--forceRefreshCounter > 0)	return;
	}
	forceRefreshCounter = 0xFFFF;
	memcpy(data_copy, sprData, sizeof(data_copy));


	for (i = 0; i < 80; i++)
	{
		HWND list = i < 40 ? hSpriteList : hSpriteList2;
		int row = i % 40;

		// yPos
		wsprintf(tmp_string, "%4d", sprData[0] & 0x03FF);
		ListView_SetItemText(list, row, 1, tmp_string);

		// xPos
		wsprintf(tmp_string, "%4d", sprData[3] & 0x03FF);
		ListView_SetItemText(list, row, 2, tmp_string);

		// size
		wsprintf(tmp_string, "%.2dx%.2d",
			TrueSize_KMod(((sprData[1] & 0x0C00) >> 10)),
			TrueSize_KMod(((sprData[1] & 0x0300) >> 8)));
		ListView_SetItemText(list, row, 3, tmp_string);

		// link
		wsprintf(tmp_string, "%.2d", sprData[1] & 0x007F);
		ListView_SetItemText(list, row, 4, tmp_string);

		// pal
		wsprintf(tmp_string, "%2d", (sprData[2] & 0x6000) >> 13);
		ListView_SetItemText(list, row, 5, tmp_string);

		// tile
		wsprintf(tmp_string, "0x%04X", (sprData[2] & 0x07FF));
		ListView_SetItemText(list, row, 6, tmp_string);

		// flags
		tmp = 0;
		/*
		if ( sprData[2]&0x8000)	tmp +=100;
		if ( sprData[2]&0x1000)	tmp +=010;
		if ( sprData[2]&0x0800)	tmp +=001;
		wsprintf(tmp_string, "%.3d", tmp);
		*/
		if (sprData[2] & 0x8000)	tmp |= 4;
		if (sprData[2] & 0x1000)	tmp |= 2;
		if (sprData[2] & 0x0800)	tmp |= 1;
		wsprintf(tmp_string, "%c%c%c", tmp & 4 ? 'P' : '0', tmp & 2 ? 'V' : '0', tmp & 1 ? 'H' : '0');
		ListView_SetItemText(list, row, 7, tmp_string);

		sprData += 4;
	}

	RedrawWindow(GetDlgItem(hSprites, IDC_SPRITES_PREVIEW2), NULL, NULL, RDW_INVALIDATE);
}


void DrawSpriteZoom_KMod(LPDRAWITEMSTRUCT hlDIS)
{
	unsigned int selIdx;
	unsigned short int *sprData, pal;
	WORD posX, posY;
	unsigned char sizeH, sizeV, zoom;
	unsigned short int numTile = 0;
	HDC hDC;
	HBITMAP hBitmap, hOldBitmap;

	selIdx = SelectedSprite_KMod(); // return item selected
	if (selIdx == -1)
		return;

	sprData = (unsigned short *)(VRam + (VDP_Reg.Spr_Att_Adr << 9));
	sprData += selIdx * 4; /* each sprite is 4 short int data */

	numTile = sprData[2] & 0x07FF;
	sizeH = TrueSize_KMod(((sprData[1] & 0x0C00) >> 10));
	sizeV = TrueSize_KMod(((sprData[1] & 0x0300) >> 8));

	zoom = (unsigned char)((hlDIS->rcItem.right - hlDIS->rcItem.left) / 32);
	zoom = (unsigned char)min(zoom, (hlDIS->rcItem.bottom - hlDIS->rcItem.top) / 32);

	hDC = CreateCompatibleDC(hlDIS->hDC);
	hBitmap = CreateCompatibleBitmap(hlDIS->hDC, 32 * zoom, 32 * zoom);
	hOldBitmap = SelectObject(hDC, hBitmap);
	FillRect(hDC, &(hlDIS->rcItem), (HBRUSH)(COLOR_3DFACE + 1));

	pal = (sprData[2] & 0x6000) >> 13;
	for (posX = 0; posX < sizeH; posX += 8)
	{
		for (posY = 0; posY < sizeV; posY += 8)
		{
			vdpdebug_drawTile(hDC, numTile++ & 0x07FF, posX*zoom, posY*zoom, (UCHAR)pal, zoom);
		}
	}


	BitBlt(
		hlDIS->hDC, // handle to destination device context
		0,  // x-coordinate of destination rectangle's upper-left
		// corner
		0,  // y-coordinate of destination rectangle's upper-left
		// corner
		32 * zoom,  // width of destination rectangle
		32 * zoom, // height of destination rectangle
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

void DumpSprite_KMod(HWND hwnd)
{
	BITMAPFILEHEADER	bmfh;
	BITMAPINFOHEADER	bmiHeader;
	RGBQUAD				bmiColors[16];
	LPBYTE				pBits;

	OPENFILENAME		szFile;
	CHAR				szFileName[MAX_PATH];
	HANDLE				hFr;
	DWORD				dwBytesToWrite, dwBytesWritten;

	COLORREF			tmpColor;
	unsigned short int	numTile;
	int					selIdx, tmp;
	unsigned char		TileData, sizeH, sizeV, posX, posY, j, pal;
	unsigned short int	*sprData;

	selIdx = SelectedSprite_KMod(); // return item selected
	if (selIdx == -1)
		return;


	szFileName[0] = 0;  /*WITHOUT THIS, CRASH */
	wsprintf(szFileName, "%s_sprite_%.2d", Rom_Name, selIdx);

	ZeroMemory(&szFile, sizeof(szFile));
	szFile.lStructSize = sizeof(szFile);
	szFile.hwndOwner = hwnd;
	szFile.lpstrFilter = "16Colors Bitmap file (*.bmp)\0*.bmp\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Dump sprite";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "bmp";

	if (GetSaveFileName(&szFile) != TRUE)   return;


	sprData = (unsigned short *)(VRam + (VDP_Reg.Spr_Att_Adr << 9));
	sprData += selIdx * 4; /* each sprite is 4 short int data */

	numTile = sprData[2] & 0x07FF;
	sizeH = TrueSize_KMod(((sprData[1] & 0x0C00) >> 10));
	sizeV = TrueSize_KMod(((sprData[1] & 0x0300) >> 8));
	pal = (sprData[2] & 0x6000) >> 13;

	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth = sizeH;
	bmiHeader.biHeight = sizeV;
	bmiHeader.biPlanes = 0;
	bmiHeader.biBitCount = 4;
	bmiHeader.biClrUsed = 16;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = sizeH*sizeV / 2;
	bmiHeader.biClrImportant = 0;


	bmfh.bfType = 0x4d42;        // 0x42 = "B" 0x4d = "M"
	bmfh.bfSize = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD) + bmiHeader.biSizeImage);
	bmfh.bfOffBits = (DWORD)(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD));


	for (j = 0; j < 16; j++)
	{
		tmpColor = vdpdebug_getColor(pal, j);
		bmiColors[j].rgbRed = (BYTE)tmpColor & 0xFF;
		tmpColor >>= 8;
		bmiColors[j].rgbGreen = (BYTE)tmpColor & 0xFF;
		tmpColor >>= 8;
		bmiColors[j].rgbBlue = (BYTE)tmpColor & 0xFF;
	}

	if (IsDlgButtonChecked(hSprites, IDC_CHECK_MAGENTAKEY))
	{
		bmiColors[0].rgbBlue = 0xff;
		bmiColors[0].rgbGreen = 0x00;
		bmiColors[0].rgbRed = 0xff;
	}

	pBits = (LPBYTE)LocalAlloc(LPTR, bmiHeader.biSizeImage);
	if (pBits == NULL)
	{
		return;
	}

	for (posX = 0; posX < sizeH; posX += 8)
	{
		for (posY = 0; posY < sizeV; posY += 8)
		{
			for (j = 0; j < 8; j++)
			{
				TileData = VRam[numTile * 32 + j * 4 + 1];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j))*sizeH / 2;
				pBits[tmp] = TileData;

				TileData = VRam[numTile * 32 + j * 4];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j))*sizeH / 2 + 1;
				pBits[tmp] = TileData;

				TileData = VRam[numTile * 32 + j * 4 + 3];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j))*sizeH / 2 + 2;
				pBits[tmp] = TileData;

				TileData = VRam[numTile * 32 + j * 4 + 2];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j))*sizeH / 2 + 3;
				pBits[tmp] = TileData;
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

	Put_Info("Sprite dumped", 1500);

}

void CopySprite_KMod(HWND hwnd)
{
	BITMAPINFOHEADER	bmiHeader;
	RGBQUAD				bmiColors[16];
	LPBYTE				pBits, pBmp;

	DWORD				dwBytesToWrite, dwBytesWritten;

	COLORREF			tmpColor;
	unsigned short int	numTile;
	int					selIdx, tmp;
	unsigned char		TileData, sizeH, sizeV, posX, posY, j, pal;
	unsigned short int* sprData;
	size_t				bmpSize;

	selIdx = SelectedSprite_KMod(); // return item selected
	if (selIdx == -1)
		return;

	sprData = (unsigned short*)(VRam + (VDP_Reg.Spr_Att_Adr << 9));
	sprData += selIdx * 4; /* each sprite is 4 short int data */

	numTile = sprData[2] & 0x07FF;
	sizeH = TrueSize_KMod(((sprData[1] & 0x0C00) >> 10));
	sizeV = TrueSize_KMod(((sprData[1] & 0x0300) >> 8));
	pal = (sprData[2] & 0x6000) >> 13;

	bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmiHeader.biWidth = sizeH;
	bmiHeader.biHeight = sizeV;
	bmiHeader.biPlanes = 0;
	bmiHeader.biBitCount = 4;
	bmiHeader.biClrUsed = 16;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = sizeH * sizeV / 2;
	bmiHeader.biClrImportant = 0;

	for (j = 0; j < 16; j++)
	{
		tmpColor = vdpdebug_getColor(pal, j);;
		bmiColors[j].rgbRed = (BYTE)tmpColor & 0xFF;
		tmpColor >>= 8;
		bmiColors[j].rgbGreen = (BYTE)tmpColor & 0xFF;
		tmpColor >>= 8;
		bmiColors[j].rgbBlue = (BYTE)tmpColor & 0xFF;
	}

	if (IsDlgButtonChecked(hSprites, IDC_CHECK_MAGENTAKEY))
	{
		bmiColors[0].rgbBlue = 0xff;
		bmiColors[0].rgbGreen = 0x00;
		bmiColors[0].rgbRed = 0xff;
	}

	pBits = (LPBYTE)LocalAlloc(LPTR, bmiHeader.biSizeImage);
	if (pBits == NULL)
	{
		return;
	}

	for (posX = 0; posX < sizeH; posX += 8)
	{
		for (posY = 0; posY < sizeV; posY += 8)
		{
			for (j = 0; j < 8; j++)
			{
				TileData = VRam[numTile * 32 + j * 4 + 1];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j)) * sizeH / 2;
				pBits[tmp] = TileData;

				TileData = VRam[numTile * 32 + j * 4];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j)) * sizeH / 2 + 1;
				pBits[tmp] = TileData;

				TileData = VRam[numTile * 32 + j * 4 + 3];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j)) * sizeH / 2 + 2;
				pBits[tmp] = TileData;

				TileData = VRam[numTile * 32 + j * 4 + 2];
				tmp = posX / 2 + ((sizeV - 1) - (posY + j)) * sizeH / 2 + 3;
				pBits[tmp] = TileData;
			}
			numTile++;
		}
	}

	bmpSize = sizeof(BITMAPINFOHEADER) + 16 * sizeof(RGBQUAD) + bmiHeader.biSizeImage;
	pBmp = (LPBYTE)LocalAlloc(LPTR, bmpSize);
	if (pBmp == NULL)
	{
		LocalFree((HLOCAL)pBits);
		return;
	}


	dwBytesWritten = 0;
	dwBytesToWrite = sizeof(BITMAPINFOHEADER);
	memcpy(pBmp + dwBytesWritten, &bmiHeader, dwBytesToWrite);

	dwBytesWritten += dwBytesToWrite;
	dwBytesToWrite = 16 * sizeof(RGBQUAD);
	memcpy(pBmp + dwBytesWritten, bmiColors, dwBytesToWrite);

	dwBytesWritten += dwBytesToWrite;
	dwBytesToWrite = bmiHeader.biSizeImage;
	memcpy(pBmp + dwBytesWritten, pBits, dwBytesToWrite);

	CopyToClipboard(CF_DIB, pBmp, bmpSize, 1);

	LocalFree((HLOCAL)pBits);
	LocalFree((HLOCAL)pBmp);

	Put_Info("Sprite copied", 1500);

}


static BOOL CALLBACK MoveSpritesFooter_KMod(HWND child, LPARAM param)
{
	RECT rect, *offset = (RECT *)param;
	GetWindowRect(child, &rect);
	MapWindowPoints(NULL, hSprites, (LPPOINT)&rect, 2);
	if (rect.top >= offset->top)
		SetWindowPos(child, NULL, rect.left, rect.top + offset->left, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	return TRUE;
}

BOOL CALLBACK SpritesDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_INITDIALOG:
		hSprites = hwnd;
		sprites_reset();
		{
			RECT rect, client, list, row, header, offset;
			int width, height;
			GetWindowRect(hSpriteList, &list);
			MapWindowPoints(NULL, hwnd, (LPPOINT)&list, 2);
			ListView_GetItemRect(hSpriteList, 0, &row, LVIR_BOUNDS);
			GetWindowRect(ListView_GetHeader(hSpriteList), &header);
			width = max(list.right - list.left, 8 * 53 + 4);
			height = (row.bottom - row.top) * 41 + header.bottom - header.top + 4;
			offset.top = list.bottom;
			offset.left = height - (list.bottom - list.top);
			EnumChildWindows(hwnd, MoveSpritesFooter_KMod, (LPARAM)&offset);
			MoveWindow(hSpriteList, list.left, list.top, width, height, TRUE);
			MoveWindow(hSpriteList2, list.left + width + 12, list.top, width, height, TRUE);
			list.right = list.left + width * 2 + 12;
			list.bottom = list.top + height;
			hSpriteGallery = CreateWindow("STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW | SS_NOTIFY,
				list.right + 12, list.top, SPRITE_COLUMNS * SPRITE_CELL_WIDTH,
				SPRITE_ROWS * SPRITE_CELL_HEIGHT, hwnd, (HMENU)IDC_SPRITES_GALLERY,
				(HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
			GetClientRect(hwnd, &client);
			GetWindowRect(hwnd, &rect);
			SetWindowPos(hwnd, NULL, 0, 0,
				list.right + 24 + SPRITE_COLUMNS * SPRITE_CELL_WIDTH + rect.right - rect.left - client.right,
				max(client.bottom + offset.left, list.top + SPRITE_ROWS * SPRITE_CELL_HEIGHT + 12) + rect.bottom - rect.top - client.bottom,
				SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			GetWindowRect(hwnd, &rect);
			minimumSpritesSize.cx = rect.right - rect.left;
			minimumSpritesSize.cy = rect.bottom - rect.top;
		}
		break;

	case WM_GETMINMAXINFO:
		if (minimumSpritesSize.cx && minimumSpritesSize.cy)
		{
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = minimumSpritesSize.cx;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = minimumSpritesSize.cy;
		}

		break;
	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_SPRITES_GALLERY)
			DrawSpriteGallery_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == IDC_SPRITES_PREVIEW2)
			DrawSpriteZoom_KMod((LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_SPRITES_GALLERY:
			{
				POINT pt;
				GetCursorPos(&pt);
				ScreenToClient(hSpriteGallery, &pt);
				SelectSpriteGallery_KMod(pt);
			}
			break;
		case IDCANCEL:
			CloseWindow_KMod(DMODE_SPRITES);
			break;
		case IDC_SPRITES_DUMP:
			DumpSprite_KMod(hSprites);
			break;
		case IDC_SPRITES_COPY:
			CopySprite_KMod(hSprites);
			break;
		case IDC_SPRITES_FRAMESTEP:
			FrameStep_KMod();
			break;
		}

		break;

	case WM_NOTIFY:
		switch (LOWORD(wParam))
		{
		case IDC_SPRITES_LIST2:
		case IDC_SPRITES_LIST:
			if (((LPNMHDR)lParam)->code == LVN_ITEMCHANGED)
			{
				NMLISTVIEW *change = (NMLISTVIEW *)lParam;
				if ((change->uChanged & LVIF_STATE) && (change->uNewState & LVIS_SELECTED) &&
					!(change->uOldState & LVIS_SELECTED))
					ListView_SetItemState(change->hdr.hwndFrom == hSpriteList ? hSpriteList2 : hSpriteList,
						-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
				planes_select_sprite(SelectedSprite_KMod());
				vdpdebug_select_sprite(SelectedSprite_KMod());
				if (hSpriteGallery) RedrawWindow(hSpriteGallery, NULL, NULL, RDW_INVALIDATE);
				RedrawWindow(GetDlgItem(hSprites, IDC_SPRITES_PREVIEW2), NULL, NULL, RDW_INVALIDATE);
			}
			break;
		}
		break;
	case WM_CLOSE:
		CloseWindow_KMod(DMODE_SPRITES);
		break;

	case WM_DESTROY:
		planes_select_sprite(-1);
		vdpdebug_select_sprite(-1);
		hSprites = NULL;
		hSpriteList = NULL;
		hSpriteList2 = NULL;
		hSpriteGallery = NULL;
		OpenedWindow_KMod[DMODE_SPRITES - 1] = FALSE;
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


void sprites_create(HINSTANCE hInstance, HWND hWndParent)
{
	hSprites = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUGSPRITES), hWndParent, SpritesDlgProc);
}

void sprites_show(BOOL visibility)
{
	ShowWindow(hSprites, visibility ? SW_SHOW : SW_HIDE);
}

void sprites_update()
{
	if (OpenedWindow_KMod[DMODE_SPRITES-1] == FALSE)	return;

	UpdateSprites_KMod();
}

void sprites_reset()
{
	SpritesInit_KMod(hSprites);
}
void sprites_destroy()
{
	ListView_DeleteAllItems(hSpriteList);
	DestroyWindow(hSprites);
}

void sprites_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "SpritesOpen",
		OpenedWindow_KMod[DMODE_SPRITES - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(hSprites, "SpritesRect", config_file);
}

void sprites_restore_window(const char *config_file)
{
	BOOL visible = GetPrivateProfileInt("DebugWindows", "SpritesOpen", 0, config_file) != 0;
	DebugWindow_RestoreGeometry(hSprites, "SpritesRect", config_file);
	{
		RECT rect;
		GetWindowRect(hSprites, &rect);
		SetWindowPos(hSprites, NULL, 0, 0, max(rect.right - rect.left, minimumSpritesSize.cx),
			max(rect.bottom - rect.top, minimumSpritesSize.cy), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	OpenedWindow_KMod[DMODE_SPRITES - 1] = visible && hSprites != NULL;
	sprites_show(visible);
}
