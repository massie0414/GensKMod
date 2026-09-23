#include <windows.h>
#include <commctrl.h>
#include <stdio.h>

#include "../gens.h"
#include "../resource.h"
#include "../vdp_io.h"

//TODO remove to use right header(s)
//#include "../kmod.h"

#include "common.h"
#include "planes.h"
#include "window_geometry.h"

#define PLANE_COUNT 2
#define PLANE_BITMAP_WIDTH 1024
#define PLANE_BITMAP_HEIGHT 2048

typedef struct PlaneExplorer
{
	HWND hwnd;
	int plane;
	BOOL show_transparence;
	unsigned int old_width;
	unsigned int old_height;
	unsigned int old_tile_height;
	unsigned char data[PLANE_BITMAP_WIDTH * PLANE_BITMAP_HEIGHT];
} PlaneExplorer;

static PlaneExplorer explorers[PLANE_COUNT];
static COLORREF plane_explorer_palette[256];

static UCHAR PlaneExplorerMode(int plane)
{
	return plane == 0 ? DMODE_PLANE_A : DMODE_PLANE_B;
}

static void PlaneExplorerInit_KMod(PlaneExplorer *explorer)
{
	HWND hexplorer = GetDlgItem(explorer->hwnd, IDC_PLANEXPLEORER_MAIN);
	RECT rc;

	SetWindowText(explorer->hwnd, explorer->plane == 0 ? "Plane A" : "Plane B");
	CheckDlgButton(explorer->hwnd, IDC_PLANEEXPLORER_TRANS,
		explorer->show_transparence ? BST_CHECKED : BST_UNCHECKED);
	explorer->old_width = 0;
	explorer->old_height = 0;
	explorer->old_tile_height = 0;
	SetDlgItemText(explorer->hwnd, IDC_PLANEEXPLORER_TILEINFO, "TILE: ");
	GetClientRect(explorer->hwnd, &rc);
	MoveWindow(hexplorer, 20, 60, max(0, rc.right - 40), max(0, rc.bottom - 80), TRUE);
	InvalidateRect(explorer->hwnd, NULL, FALSE);
}

static void PlaneExplorer_UpdatePalette(void)
{
	COLORREF * cr = &plane_explorer_palette[0];
	COLORREF col;
	unsigned short * pal = (unsigned short *)(&CRam[0]);
	int i;
	static const COLORREF normal_pal[] =
	{
		0x00000000,
		0x00000011,
		0x00000022,
		0x00000033,
		0x00000044,
		0x00000055,
		0x00000066,
		0x00000077,
		0x00000088,
		0x00000099,
		0x000000AA,
		0x000000BB,
		0x000000CC,
		0x000000DD,
		0x000000EE,
		0x000000FF
	};

	for (i = 0; i < 64; i++)
	{
		unsigned short p = *pal++;
		col = normal_pal[(p >> 8) & 0xF] << 0;
		col |= normal_pal[(p >> 4) & 0xF] << 8;
		col |= normal_pal[(p >> 0) & 0xF] << 16;
		*cr++ = col;
	}

	plane_explorer_palette[253] = 0x00333333;
	plane_explorer_palette[254] = 0x00444444;
	plane_explorer_palette[255] = 0x00555555;
}

union PATTERN_NAME
{
	struct
	{
		unsigned short tile_index : 11;
		unsigned short h_flip : 1;
		unsigned short v_flip : 1;
		unsigned short pal_index : 2;
		unsigned short priority : 1;
	};
	unsigned short word;
};

static unsigned short byte_swap(unsigned short w)
{
	return (w >> 8) | (w << 8);
}

static void PlaneExplorer_DrawTile(PlaneExplorer *explorer, unsigned short name_word, unsigned int x, unsigned int y, int transcolor)
{
	union PATTERN_NAME name;
	unsigned int tile_height = ((VDP_Reg.Set4 & 0x6) == 6) ? 16 : 8;
	unsigned char * ptr = &explorer->data[y * 1024 * tile_height + x * 8];
	unsigned int j, k;
	unsigned int * tile_data;
	int stride = 1024;
	unsigned char pal_index;

	name.word = name_word;
	tile_data = (unsigned int *)(&VRam[(name.tile_index * tile_height * 4) & 0xFFFF]);
	pal_index = (unsigned char)name.pal_index << 4;

	if (name.v_flip)
	{
		ptr += (tile_height - 1) * stride;
		stride = -stride;
	}

	if (name.h_flip)
	{
		for (j = 0; j < tile_height; j++)
		{
			unsigned int tile_row = tile_data[j];
			ptr[4] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[5] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[6] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[7] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[0] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[1] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[2] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[3] = (tile_row & 0xF) | pal_index;
			if (transcolor != -1)
			{
				for (k = 0; k < 8; k++)
				{
					if (ptr[k] == pal_index)
					{
						ptr[k] = transcolor;
					}
				}
			}
			ptr += stride;
		}
	}
	else
	{
		for (j = 0; j < tile_height; j++)
		{
			unsigned int tile_row = tile_data[j];
			ptr[3] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[2] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[1] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[0] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[7] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[6] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[5] = (tile_row & 0xF) | pal_index;    tile_row >>= 4;
			ptr[4] = (tile_row & 0xF) | pal_index;
			if (transcolor != -1)
			{
				for (k = 0; k < 8; k++)
				{
					if (ptr[k] == pal_index)
					{
						ptr[k] = transcolor;
					}
				}
			}
			ptr += stride;
		}
	}
}

static void PlaneExplorer_UpdateBitmap(PlaneExplorer *explorer)
{
	unsigned int i, j;

	unsigned int plane_width = 32 + (VDP_Reg.Scr_Size & 0x3) * 32;
	unsigned int plane_height = 32 + ((VDP_Reg.Scr_Size >> 4) & 0x3) * 32;
	unsigned int plane_a_base = (VDP_Reg.Pat_ScrA_Adr & 0x38) << 10;
	unsigned int plane_b_base = (VDP_Reg.Pat_ScrB_Adr & 0x7) << 13;
	unsigned int plane_base = explorer->plane == 0 ? plane_a_base : plane_b_base;
	unsigned int tile_height = ((VDP_Reg.Set4 & 0x6) == 6) ? 16 : 8;

	if (plane_width != explorer->old_width ||
		plane_height != explorer->old_height ||
		tile_height != explorer->old_tile_height)
	{
		explorer->old_width = plane_width;
		explorer->old_height = plane_height;
		explorer->old_tile_height = tile_height;
		for (i = 0; i < PLANE_BITMAP_HEIGHT; i++)
		{
			for (j = 0; j < 1024; j++)
			{
				explorer->data[i * 1024 + j] = (unsigned char)(((j ^ i) >> 2) & 1) + 253;
			}
		}
	}

	for (j = 0; j < plane_height; j++)
	{
		for (i = 0; i < plane_width; i++)
		{
			int trans_color = explorer->show_transparence ? (unsigned char)(((j ^ i) >> 1) & 1) + 254 : -1;
			unsigned int address = (plane_base + (j * plane_width + i) * 2) & 0xFFFF;
			PlaneExplorer_DrawTile(explorer, *(unsigned short *)&VRam[address], i, j, trans_color);
		}
	}
}

static void PlaneExplorerPaint_KMod(PlaneExplorer *explorer, LPDRAWITEMSTRUCT lpdi)
{
	struct BMI_LOCAL
	{
		BITMAPINFOHEADER hdr;
		COLORREF         palette[256];
	};

	struct BMI_LOCAL bmi =
	{
		{
			sizeof(BITMAPINFOHEADER),
			128 * 8,
			-PLANE_BITMAP_HEIGHT,
			1,
			8,
			BI_RGB,
			0,
			250,
			250,
			256,
			256,
		},
		{
			0x00FF0055,
		}
	};

	unsigned int plane_a_base = (VDP_Reg.Pat_ScrA_Adr & 0x38) << 10;
	unsigned int plane_b_base = (VDP_Reg.Pat_ScrB_Adr & 0x7) << 13;

	char buffer[1024];
	PlaneExplorer_UpdatePalette();
	PlaneExplorer_UpdateBitmap(explorer);

	memcpy(bmi.palette, plane_explorer_palette, sizeof(bmi.palette));

	SetDIBitsToDevice(
		lpdi->hDC,
		lpdi->rcItem.left, lpdi->rcItem.top,
		lpdi->rcItem.right - lpdi->rcItem.left,
		lpdi->rcItem.bottom - lpdi->rcItem.top,
		lpdi->rcItem.left, lpdi->rcItem.top,
		lpdi->rcItem.top, lpdi->rcItem.bottom - lpdi->rcItem.top,
		explorer->data,
		(const BITMAPINFO *)&bmi,
		DIB_RGB_COLORS);

	wsprintf(buffer, "Width: %d Height: %d  Plane A Base: 0x%04X  Plane B Base: 0x%04X  Mode: %s",
		32 + (VDP_Reg.Scr_Size & 0x3) * 32,
		32 + ((VDP_Reg.Scr_Size >> 4) & 0x3) * 32,
		plane_a_base, plane_b_base,
		((VDP_Reg.Set4 & 0x6) == 2) ? "Interlaced" :
		((VDP_Reg.Set4 & 0x6) == 6) ? "Double interlaced" : "Normal");
	SetDlgItemText(explorer->hwnd, IDC_PLANEEXPLORER_PROPS, buffer);
}

static void PlaneExplorer_GetTipText(PlaneExplorer *explorer, int x, int y, char * buffer)
{
	int plane_size_x = 32 + (VDP_Reg.Scr_Size & 0x3) * 32;
	int plane_size_y = 32 + ((VDP_Reg.Scr_Size >> 4) & 0x3) * 32;


	unsigned int plane_a_base = (VDP_Reg.Pat_ScrA_Adr & 0x38) << 10;
	unsigned int plane_b_base = (VDP_Reg.Pat_ScrB_Adr & 0x7) << 13;
	unsigned int base = explorer->plane ? plane_b_base : plane_a_base;
	char plane_char = explorer->plane ? 'B' : 'A';
	unsigned int tile_height = ((VDP_Reg.Set4 & 0x6) == 6) ? 16 : 8;
	unsigned int tile_addr = (base + ((y / tile_height) * plane_size_x + (x >> 3)) * 2) & 0xFFFF;
	union PATTERN_NAME name;


	if (x < 0 || y < 0 || x >= (plane_size_x * 8) ||
		y >= (int)(plane_size_y * tile_height))
	{
		buffer[0] = 0;
		return;
	}

	name.word = *(unsigned short *)(&VRam[tile_addr]);

	wsprintf(buffer, "%d, %d in plane %c @ 0x%04X is @ 0x%04X = 0x%04X [tile %d, pal %d,%s%s prior %d]",
		x >> 3,
		y / tile_height,
		plane_char,
		base,
		tile_addr,
		name.word,
		name.tile_index,
		name.pal_index,
		name.h_flip ? " HFLIP," : "",
		name.v_flip ? " VFLIP," : "",
		name.priority
		);
}

static INT_PTR CALLBACK PlaneExplorerDialogProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	PlaneExplorer *explorer = (PlaneExplorer *)GetWindowLongPtr(hwnd, DWLP_USER);
	if (Message == WM_INITDIALOG)
	{
		explorer = (PlaneExplorer *)lParam;
		explorer->hwnd = hwnd;
		SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)explorer);
	}
	if (!explorer) return FALSE;

	switch (Message)
	{
	case WM_INITDIALOG:
		PlaneExplorerInit_KMod(explorer);
		break;

	case WM_DRAWITEM:
		if (wParam != IDC_PLANEXPLEORER_MAIN) return FALSE;
		PlaneExplorerPaint_KMod(explorer, (LPDRAWITEMSTRUCT)lParam);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_PLANEEXPLORER_TRANS:
			explorer->show_transparence = (IsDlgButtonChecked(hwnd, IDC_PLANEEXPLORER_TRANS) == BST_CHECKED);
			InvalidateRect(hwnd, NULL, FALSE);
			break;
		case IDCANCEL:
			CloseWindow_KMod(PlaneExplorerMode(explorer->plane));
			break;
		default:
			break;
		}
		break;

	case WM_SIZE:
	{
		HWND hexplorer = GetDlgItem(hwnd, IDC_PLANEXPLEORER_MAIN);
		MoveWindow(hexplorer, 20, 60, max(0, (int)LOWORD(lParam) - 40), max(0, (int)HIWORD(lParam) - 80), TRUE);
		break;
	}

	case WM_CLOSE:
		CloseWindow_KMod(PlaneExplorerMode(explorer->plane));
		break;

	case WM_DESTROY:
		OpenedWindow_KMod[PlaneExplorerMode(explorer->plane) - 1] = FALSE;
		HandleWindow_KMod[PlaneExplorerMode(explorer->plane) - 1] = NULL;
		explorer->hwnd = NULL;
		break;

	case WM_MOUSELEAVE:
		SetDlgItemText(hwnd, IDC_PLANEEXPLORER_TILEINFO, "TILE: ");
		return FALSE;

	case WM_MOUSEMOVE:
	{
		HWND hexplorer = GetDlgItem(hwnd, IDC_PLANEXPLEORER_MAIN);
		int x = (short)(lParam);
		int y = (short)(lParam >> 16);
		char buffer[180] = "TILE: ";
		RECT rc1;
		POINT pt;
		TRACKMOUSEEVENT tme = { sizeof(tme) };
		tme.hwndTrack = hwnd;
		tme.dwFlags = TME_LEAVE;
		TrackMouseEvent(&tme);
		pt.x = x;
		pt.y = y;
		ClientToScreen(hwnd, &pt);
		ScreenToClient(hexplorer, &pt);
		GetClientRect(hexplorer, &rc1);
		if (PtInRect(&rc1, pt))
		{
			PlaneExplorer_GetTipText(explorer, pt.x, pt.y, buffer + 6);
		}
		SetDlgItemText(hwnd, IDC_PLANEEXPLORER_TILEINFO, buffer);
		return FALSE;
	}

	default:
		return FALSE;
	}

	return TRUE;
}

void planes_create(HINSTANCE hInstance, HWND hWndParent)
{
	int plane;
	for (plane = 0; plane < PLANE_COUNT; ++plane)
	{
		PlaneExplorer *explorer = &explorers[plane];
		explorer->plane = plane;
		explorer->hwnd = CreateDialogParam(hInstance, MAKEINTRESOURCE(IDD_DEBUGPLANEEXPLORER),
			hWndParent, PlaneExplorerDialogProc, (LPARAM)explorer);
		HandleWindow_KMod[PlaneExplorerMode(plane) - 1] = explorer->hwnd;
	}
}

void planes_show(int plane, BOOL visibility)
{
	if (plane < 0 || plane >= PLANE_COUNT) return;
	ShowWindow(explorers[plane].hwnd, visibility ? SW_SHOW : SW_HIDE);
}

void planes_update()
{
	int plane;
	for (plane = 0; plane < PLANE_COUNT; ++plane)
	{
		if (OpenedWindow_KMod[PlaneExplorerMode(plane) - 1] && explorers[plane].hwnd)
			RedrawWindow(GetDlgItem(explorers[plane].hwnd, IDC_PLANEXPLEORER_MAIN),
				NULL, NULL, RDW_INVALIDATE);
	}
}

void planes_reset()
{
	int plane;
	for (plane = 0; plane < PLANE_COUNT; ++plane)
		if (explorers[plane].hwnd) PlaneExplorerInit_KMod(&explorers[plane]);
}

void planes_destroy()
{
	int plane;
	for (plane = 0; plane < PLANE_COUNT; ++plane)
		if (explorers[plane].hwnd) DestroyWindow(explorers[plane].hwnd);
}

/* Keep session state separate from the debug options dialog's Apply/Cancel. */
void planes_save_visibility(const char *config_file)
{
	WritePrivateProfileString("DebugWindows", "PlaneAOpen",
		OpenedWindow_KMod[DMODE_PLANE_A - 1] ? "1" : "0", config_file);
	WritePrivateProfileString("DebugWindows", "PlaneBOpen",
		OpenedWindow_KMod[DMODE_PLANE_B - 1] ? "1" : "0", config_file);
	DebugWindow_SaveGeometry(explorers[0].hwnd, "PlaneARect", config_file);
	DebugWindow_SaveGeometry(explorers[1].hwnd, "PlaneBRect", config_file);
}

void planes_restore_visibility(const char *config_file)
{
	int plane;
	for (plane = 0; plane < PLANE_COUNT; ++plane)
	{
		BOOL visible = GetPrivateProfileInt("DebugWindows",
			plane == 0 ? "PlaneAOpen" : "PlaneBOpen", 0, config_file) != 0;
		DebugWindow_RestoreGeometry(explorers[plane].hwnd,
			plane == 0 ? "PlaneARect" : "PlaneBRect", config_file);
		OpenedWindow_KMod[PlaneExplorerMode(plane) - 1] =
			visible && explorers[plane].hwnd != NULL;
		planes_show(plane, visible);
	}
}
