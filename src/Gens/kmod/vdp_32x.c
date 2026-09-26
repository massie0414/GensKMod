#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../vdp_32X.h"
#include "../vdp_rend.h"
#include "../vdp_io.h"
#include "../G_gfx.h" //used for Put_Info

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "utils.h"
#include "vdp_32x.h"
#include "window_geometry.h"

static HWND h32X_VDP;
static SIZE minimum32XVDPSize;
#define PALETTE32X_COLUMNS 8
#define PALETTE32X_ROWS 32
static long palH, palV;
static WNDPROC paletteWindowProc;
static int paletteIndex = -1;
static int hoverBank = -1, hoverX, hoverY;
static WNDPROC framebufferWindowProc[2];
static BOOL Sample32XPixel(unsigned bank, int x, int y, int *index, unsigned *color);

static void Update32XPaletteInfo(void)
{
    char text[100], old[100], indexText[32];
    unsigned color = 0;
    BOOL valid = paletteIndex >= 0;
    if (hoverBank >= 0)
        valid = Sample32XPixel(hoverBank, hoverX, hoverY, &paletteIndex, &color);
    else if (valid) color = _32X_VDP_CRam[paletteIndex];
    if (!valid)
        strcpy(text, "Index: --\r\nR: --\r\nG: --\r\nB: --\r\nPriority: --");
    else
    {
        if (paletteIndex >= 0)
            sprintf(indexText, "%u (0x%02X)", (unsigned)paletteIndex, (unsigned)paletteIndex);
        else strcpy(indexText, "--");
        sprintf(text, "Index: %s\r\nR: %u\r\nG: %u\r\nB: %u\r\nPriority: %u",
            indexText, color & 31, (color >> 5) & 31, (color >> 10) & 31, (color >> 15) & 1);
    }
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, old, sizeof(old));
    if (strcmp(text, old)) SetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text);
}

static LRESULT CALLBACK Palette32XWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_MOUSEMOVE)
    {
        RECT rect;
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        int cellWidth, cellHeight;
        GetClientRect(hwnd, &rect);
        cellWidth = rect.right / PALETTE32X_COLUMNS;
        cellHeight = rect.bottom / PALETTE32X_ROWS;
        if (cellWidth > 0 && cellHeight > 0 && x >= 0 && y >= 0 &&
            x < cellWidth * PALETTE32X_COLUMNS && y < cellHeight * PALETTE32X_ROWS)
        {
            hoverBank = -1;
            paletteIndex = (y / cellHeight) * PALETTE32X_COLUMNS + x / cellWidth;
            Update32XPaletteInfo();
        }
    }
    return CallWindowProc(paletteWindowProc, hwnd, message, wParam, lParam);
}

void Update32X_VDP_KMod()
{
    const char *names[] = { "blank", "256-color", "direct color", "RLE" };
    char label[100], old[100];
    unsigned bank;
    Update32XPaletteInfo();
    for (bank = 0; bank < 2; ++bank)
    {
        int id = bank ? IDC_32XVDP_LABEL1 : IDC_32XVDP_LABEL0;
        sprintf(label, "FB%u (%s) - %s", bank, names[_32X_VDP.Mode & 3],
            ((_32X_VDP.State & 1) == bank) ? "displayed" : "other");
        GetDlgItemText(h32X_VDP, id, old, sizeof(old));
        if (strcmp(old, label)) SetDlgItemText(h32X_VDP, id, label);
    }
	RedrawWindow(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES), NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES2), NULL, NULL, RDW_INVALIDATE);
	RedrawWindow(GetDlgItem(h32X_VDP, IDC_32XVDP_PAL), NULL, NULL, RDW_INVALIDATE);

}

void Draw32XPal_KMod(LPDRAWITEMSTRUCT hlDIS)
{

	unsigned char i, j, tone;
	HBRUSH newBrush = NULL;
	HPEN hPen, hPenOld;
	RECT rc;
	LONG pix, h;
	COLORREF col;

	palV = (hlDIS->rcItem.bottom - hlDIS->rcItem.top) / PALETTE32X_ROWS;
	palH = (hlDIS->rcItem.right - hlDIS->rcItem.left) / PALETTE32X_COLUMNS;

	for (j = 0; j < PALETTE32X_ROWS; j++)
	{
		rc.top = j* palV;
		rc.bottom = rc.top + palV;

		for (i = 0; i < PALETTE32X_COLUMNS; i++)
		{
			if (newBrush)	DeleteObject((HGDIOBJ)newBrush);
			rc.left = i*palH;
			rc.right = rc.left + palH;

			// COLORREF = 0x00bbggrr
			// pix = bgr (3*5bit)
			pix = _32X_VDP_CRam[i + j * PALETTE32X_COLUMNS];
			col = 0x000000;
			tone = (pix >> 10) & 0x1F;
			col |= (tone << 3);
			col <<= 8;
			tone = (pix >> 5) & 0x1F;
			col |= (tone << 3);
			col <<= 8;
			tone = pix & 0x1F;
			col |= (tone << 3);

			newBrush = CreateSolidBrush(col);
			FillRect(hlDIS->hDC, &rc, newBrush);

			if (pix & 0x8000) /* Priority bit. */
			{
				//FillRect(hlDIS->hDC, &rc, (HBRUSH) (COLOR_WINDOWFRAME+1));

				hPen = CreatePen(PS_DOT, 1, col);
				hPenOld = SelectObject(hlDIS->hDC, hPen);

				for (h = rc.top; h < rc.bottom; h += 4)
				{
					MoveToEx(hlDIS->hDC, rc.left, h, NULL);
					LineTo(hlDIS->hDC, rc.right, h);
					MoveToEx(hlDIS->hDC, rc.left + 2, h + 2, NULL);
					LineTo(hlDIS->hDC, rc.right, h + 2);
				}

				SelectObject(hlDIS->hDC, hPenOld);
				DeleteObject(hPen);

			}

		}
	}

	if (newBrush)	DeleteObject((HGDIOBJ)newBrush);



}

/* Both owner-drawn views run synchronously on the emulation/UI thread.
 * Reuse one top-down 32-bit DIB buffer; no per-pixel GDI calls or per-paint
 * bitmap/DC allocation. SetDIBitsToDevice consumes the pixels before returning.
 */
#define VDP32X_VIEW_WIDTH 320
#define VDP32X_VIEW_HEIGHT 240
#define VDP32X_FB_WORDS 0x10000
static DWORD vdp32x_pixels[VDP32X_VIEW_WIDTH * VDP32X_VIEW_HEIGHT];

/* Convert the emulator's adjusted RGB555/RGB565 output palette to a DIB.
 * No MD_Screen access: these previews show only the selected 32X framebuffer.
 */
static DWORD Color32X_KMod(WORD color)
{
    unsigned r = (color >> ((Mode_555 & 1) ? 10 : 11)) & 31;
    unsigned g = (color >> 5) & ((Mode_555 & 1) ? 31 : 63);
    unsigned b = color & 31;
    r = (r << 3) | (r >> 2);
    g = (Mode_555 & 1) ? ((g << 3) | (g >> 2)) : ((g << 2) | (g >> 4));
    b = (b << 3) | (b >> 2);
    return (r << 16) | (g << 8) | b;
}

static void Decode32X_KMod(unsigned bank, DWORD *pixels)
{
    const WORD *fb = (const WORD *)(_32X_VDP_Ram + bank * 0x20000);
    unsigned mode = _32X_VDP.Mode & 3;
    unsigned y, x, address, word, index, run;
    unsigned lines = (_32X_Started || CD_32X_Active) ? VDP_Num_Vis_Lines : 0;
    if (lines > VDP32X_VIEW_HEIGHT) lines = VDP32X_VIEW_HEIGHT;
    memset(pixels, 0, VDP32X_VIEW_WIDTH * VDP32X_VIEW_HEIGHT * sizeof(*pixels));
    if (!mode) return;
    for (y = 0; y < lines; ++y)
    {
        address = fb[y];
        if (mode == 3) /* RLE: high byte = run length minus one, low = index. */
        {
            x = 0;
            while (x < VDP32X_VIEW_WIDTH && address < VDP32X_FB_WORDS)
            {
                word = fb[address++];
                run = (word >> 8) + 1;
                if (run > VDP32X_VIEW_WIDTH - x) run = VDP32X_VIEW_WIDTH - x;
                while (run--) pixels[y * VDP32X_VIEW_WIDTH + x++] =
                    Color32X_KMod(_32X_Palette_16B[_32X_VDP_CRam[word & 255]]);
            }
        }
        else for (x = 0; x < VDP32X_VIEW_WIDTH; ++x)
        {
            index = x + ((mode == 1 && (_32X_VDP.Mode & 0x10000)) ? 1 : 0);
            word = address + ((mode == 1) ? index / 2 : x);
            if (word >= VDP32X_FB_WORDS) break;
            word = fb[word];
            if (mode == 1) /* Packed pixels: high byte is the left pixel. */
                word = _32X_VDP_CRam[(index & 1) ? (word & 255) : (word >> 8)];
            pixels[y * VDP32X_VIEW_WIDTH + x] = Color32X_KMod(_32X_Palette_16B[word]);
        }
    }
}

/* Match the preview's addressing, including packed-pixel shift and RLE runs. */
static BOOL Sample32XPixel(unsigned bank, int x, int y, int *index, unsigned *color)
{
    const WORD *fb = (const WORD *)(_32X_VDP_Ram + bank * 0x20000);
    unsigned mode = _32X_VDP.Mode & 3;
    unsigned address, word, pixel, end = 0;
    BOOL raw = IsDlgButtonChecked(h32X_VDP, IDC_32XVDP_FB1) == BST_CHECKED;
    BOOL lineTable = IsDlgButtonChecked(h32X_VDP, IDC_32XVDP_FB2) == BST_CHECKED;
    *index = -1;
    if (x < 0 || y < 0 || x >= VDP32X_VIEW_WIDTH || y >= VDP32X_VIEW_HEIGHT) return FALSE;
    if (raw || lineTable)
    {
        address = (lineTable ? fb[y] : 256 + y * VDP32X_VIEW_WIDTH) + x;
        if (address >= VDP32X_FB_WORDS) return FALSE;
        *color = fb[address];
        return TRUE;
    }
    if ((!_32X_Started && !CD_32X_Active) || !mode || y >= VDP_Num_Vis_Lines) return FALSE;
    address = fb[y];
    if (mode == 3)
    {
        do
        {
            if (address >= VDP32X_FB_WORDS) return FALSE;
            word = fb[address++];
            end += (word >> 8) + 1;
        } while (end <= (unsigned)x);
        *index = word & 255;
    }
    else
    {
        pixel = x + ((mode == 1 && (_32X_VDP.Mode & 0x10000)) ? 1 : 0);
        address += mode == 1 ? pixel / 2 : x;
        if (address >= VDP32X_FB_WORDS) return FALSE;
        word = fb[address];
        if (mode == 2) { *color = word; return TRUE; }
        *index = (pixel & 1) ? (word & 255) : (word >> 8);
    }
    *color = _32X_VDP_CRam[*index];
    return TRUE;
}

static LRESULT CALLBACK Framebuffer32XWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    unsigned bank = GetDlgCtrlID(hwnd) == IDC_32XVDP_TILES2 ? 1 : 0;
    if (message == WM_MOUSEMOVE)
    {
        hoverBank = bank;
        hoverX = (short)LOWORD(lParam);
        hoverY = (short)HIWORD(lParam);
        Update32XPaletteInfo();
    }
    return CallWindowProc(framebufferWindowProc[bank], hwnd, message, wParam, lParam);
}

static void Draw32XBitmap_KMod(LPDRAWITEMSTRUCT item, unsigned bank)
{
    const WORD *fb = (const WORD *)(_32X_VDP_Ram + bank * 0x20000);
    BITMAPINFO bmi = {0};
    unsigned int x, y, address;
    BOOL raw = IsDlgButtonChecked(h32X_VDP, IDC_32XVDP_FB1) == BST_CHECKED;
    BOOL lineTable = IsDlgButtonChecked(h32X_VDP, IDC_32XVDP_FB2) == BST_CHECKED;
    int width = item->rcItem.right - item->rcItem.left;
    int height = item->rcItem.bottom - item->rcItem.top;
    if (width <= 0 || height <= 0) return;

    if (!raw && !lineTable) Decode32X_KMod(bank, vdp32x_pixels);
    else for (y = 0; y < VDP32X_VIEW_HEIGHT; ++y)
    {
        address = lineTable ? fb[y] : 256 + y * VDP32X_VIEW_WIDTH;
        for (x = 0; x < VDP32X_VIEW_WIDTH; ++x)
        {
            WORD pix = address + x < VDP32X_FB_WORDS ? fb[address + x] : 0;
            vdp32x_pixels[y * VDP32X_VIEW_WIDTH + x] =
                ((DWORD)(pix & 31) << 19) |
                ((DWORD)((pix >> 5) & 31) << 11) |
                ((DWORD)((pix >> 10) & 31) << 3);
        }
    }

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = VDP32X_VIEW_WIDTH;
    bmi.bmiHeader.biHeight = -VDP32X_VIEW_HEIGHT;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    if (width > VDP32X_VIEW_WIDTH) width = VDP32X_VIEW_WIDTH;
    if (height > VDP32X_VIEW_HEIGHT) height = VDP32X_VIEW_HEIGHT;
    SetDIBitsToDevice(item->hDC, item->rcItem.left, item->rcItem.top,
        width, height, 0, VDP32X_VIEW_HEIGHT - height, 0, VDP32X_VIEW_HEIGHT,
        vdp32x_pixels, &bmi, DIB_RGB_COLORS);
}

void Draw32XVDP_KMod(LPDRAWITEMSTRUCT item)
{
    Draw32XBitmap_KMod(item, 1);
}

void Draw32XVDPRaw_KMod(LPDRAWITEMSTRUCT item)
{
    Draw32XBitmap_KMod(item, 0);
}


void Dump32XCRAM_KMod(HWND hwnd)
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
	szFile.lpstrFilter = "32X CRAM dump (*.bin)\0*.bin\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Dump 32X CRAM";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "bin";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	hFr = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFr == INVALID_HANDLE_VALUE)
		return;

	dwBytesToWrite = 0x100;
	WriteFile(hFr, _32X_VDP_CRam_Ajusted, dwBytesToWrite, &dwBytesWritten, NULL);

	CloseHandle(hFr);

	Put_Info("32X CRAM dumped", 1500);
}

void Dump32XVDP_KMod(HWND hwnd)
{
	OPENFILENAME szFile;
	char szFileName[MAX_PATH];
	HANDLE hFr;
	DWORD dwBytesToWrite, dwBytesWritten;

	ZeroMemory(&szFile, sizeof(szFile));
	szFileName[0] = 0;  /*WITHOUT THIS, CRASH */

	strcpy(szFileName, Rom_Name);
	strcat(szFileName, "_VDP");

	szFile.lStructSize = sizeof(szFile);
	szFile.hwndOwner = hwnd;
	szFile.lpstrFilter = "Frame Buffer dump (*.bin)\0*.bin\0\0";
	szFile.lpstrFile = szFileName;
	szFile.nMaxFile = sizeof(szFileName);
	szFile.lpstrFileTitle = (LPSTR)NULL;
	szFile.lpstrInitialDir = (LPSTR)NULL;
	szFile.lpstrTitle = "Dump 32X Frame buffer";
	szFile.Flags = OFN_EXPLORER | OFN_LONGNAMES | OFN_NONETWORKBUTTON |
		OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	szFile.lpstrDefExt = "bin";

	if (GetSaveFileName(&szFile) != TRUE)   return;

	hFr = CreateFile(szFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFr == INVALID_HANDLE_VALUE)
		return;

	dwBytesToWrite = 0x100 * 1024;
	WriteFile(hFr, _32X_VDP_Ram, dwBytesToWrite, &dwBytesWritten, NULL);

	CloseHandle(hFr);

	Put_Info("32X Frame buffer dumped", 1500);
}


BOOL CALLBACK _32X_VDPDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{

	switch (Message)
	{
	case WM_INITDIALOG:
		h32X_VDP = hwnd;
        paletteIndex = -1;
        hoverBank = -1;
        framebufferWindowProc[0] = (WNDPROC)SetWindowLongPtr(
            GetDlgItem(hwnd, IDC_32XVDP_TILES), GWLP_WNDPROC, (LONG_PTR)Framebuffer32XWindowProc);
        framebufferWindowProc[1] = (WNDPROC)SetWindowLongPtr(
            GetDlgItem(hwnd, IDC_32XVDP_TILES2), GWLP_WNDPROC, (LONG_PTR)Framebuffer32XWindowProc);
        paletteWindowProc = (WNDPROC)SetWindowLongPtr(
            GetDlgItem(hwnd, IDC_32XVDP_PAL), GWLP_WNDPROC, (LONG_PTR)Palette32XWindowProc);
        Update32XPaletteInfo();
		{
			RECT rect;
			GetWindowRect(hwnd, &rect);
			minimum32XVDPSize.cx = rect.right - rect.left;
			minimum32XVDPSize.cy = rect.bottom - rect.top;
		}
		CheckRadioButton(hwnd, IDC_32XVDP_FB0, IDC_32XVDP_FB2, IDC_32XVDP_FB0);
		break;

	case WM_GETMINMAXINFO:
		if (minimum32XVDPSize.cx && minimum32XVDPSize.cy)
		{
			((MINMAXINFO *)lParam)->ptMinTrackSize.x = minimum32XVDPSize.cx;
			((MINMAXINFO *)lParam)->ptMinTrackSize.y = minimum32XVDPSize.cy;
		}
		break;

	case WM_DRAWITEM:
		if ((UINT)wParam == IDC_32XVDP_TILES)
			Draw32XVDPRaw_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == IDC_32XVDP_TILES2)
			Draw32XVDP_KMod((LPDRAWITEMSTRUCT)lParam);
		else if ((UINT)wParam == IDC_32XVDP_PAL)
			Draw32XPal_KMod((LPDRAWITEMSTRUCT)lParam);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_32XVDP_DUMP:
			Dump32XVDP_KMod(h32X_VDP);
			break;
		case IDC_32XVDP_CRAM:
			Dump32XCRAM_KMod(h32X_VDP);
			break;
		case IDC_32XVDP_FB0:
		case IDC_32XVDP_FB1:
		case IDC_32XVDP_FB2:
			Update32X_VDP_KMod();
			break;
		}
		break;

	case WM_CLOSE:
		CloseWindow_KMod(DMODE_32_VDP);
		break;

	case WM_DESTROY:
        paletteIndex = -1;
		h32X_VDP = NULL;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}


void vdp32x_create(HINSTANCE hInstance, HWND hWndParent)
{
	minimum32XVDPSize.cx = minimum32XVDPSize.cy = 0;
	h32X_VDP = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DEBUG32X_VDP), hWndParent, _32X_VDPDlgProc);
}

void vdp32x_show(BOOL visibility)
{
	ShowWindow(h32X_VDP, visibility ? SW_SHOW : SW_HIDE);
}

void vdp32x_update()
{
	if (OpenedWindow_KMod[DMODE_32_VDP-1] == FALSE)	return;

	Update32X_VDP_KMod();
}

void vdp32x_reset()
{

}
void vdp32x_destroy()
{
	if (h32X_VDP) DestroyWindow(h32X_VDP);
}


void vdp32x_save_window(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "32XVDPOpen",
		OpenedWindow_KMod[DMODE_32_VDP - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(h32X_VDP, "32XVDPRect", config_file);
}

void vdp32x_restore_window(const char *config_file)
{
	BOOL visible = GetPrivateProfileInt("DebugWindows", "32XVDPOpen", 0, config_file) != 0;
	DebugWindow_RestoreGeometry(h32X_VDP, "32XVDPRect", config_file);
	OpenedWindow_KMod[DMODE_32_VDP - 1] = visible && h32X_VDP != NULL;
	vdp32x_show(visible);
}
