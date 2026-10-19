/* Build the Release solution first, then use an x86 VS developer prompt:
 * cl /nologo /TC tests\vdp_window_test.c /Fo"%TEMP%\vdp_window_test.obj" /Fe"%TEMP%\vdp_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib comdlg32.lib
 * "%TEMP%\vdp_window_test.exe"
 */
#include <assert.h>
#include "../src/Gens/kmod/vdp.c"

unsigned char VRam[65536], CRam[256];
struct Reg_VDP_Type VDP_Reg;
UCHAR ActivePal, OpenedWindow_KMod[WIN_NUMBER];
int Paused = 1, CRam_Flag;
HACCEL hAccelTable;
char Rom_Name[512];
void Put_Info(const char *message, int duration) { }
void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    vdpdebug_show(FALSE);
}

int main(void)
{
    char temp[MAX_PATH], config[MAX_PATH];
    RECT expected, actual;
    MINMAXINFO limits = {0};
    MSG msg;
    assert(GetTempPath(sizeof(temp), temp));
    assert(GetTempFileName(temp, "vdp", 0, config));
    vdpdebug_create(GetModuleHandle(NULL), NULL);
    assert(hVDP);
    {
        RECT tiles, client;
        HDC dc = GetDC(hVDP), memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, 512, 256);
        HBITMAP previous = SelectObject(memory, bitmap);
        DRAWITEMSTRUCT draw = {0};
        POINT lastTile;
        GetClientRect(GetDlgItem(hVDP, IDC_VDP_TILES), &tiles);
        assert(tiles.right == 512 && tiles.bottom == 256);
        assert(!GetDlgItem(hVDP, IDC_VDP_SCROLL));
        GetClientRect(hVDP, &client);
        assert(rcTiles.right <= client.right && rcTiles.bottom <= client.bottom);
        memset(VRam + 2047 * 32, 0xFF, 32);
        pal_KMod = VDP_PALETTE_MONO;
        draw.hDC = memory;
        draw.rcItem = tiles;
        /* Running and paused updates must both paint the final tile. */
        for (Paused = 0; Paused <= 1; Paused++)
        {
            PatBlt(memory, 0, 0, 512, 256, BLACKNESS);
            DrawVDP_Tiles_KMod(&draw);
            assert(GetPixel(memory, 511, 255) == RGB(255, 255, 255));
        }
        {
            unsigned short *sat = (unsigned short *)(VRam + 0xF000);
            VDP_Reg.Set4 = 1; VDP_Reg.Spr_Att_Adr = 0x78;
            sat[0] = 128; sat[1] = 0x0400; sat[2] = 63; sat[3] = 128;
            vdpdebug_select_sprite(0);
            DrawVDP_Tiles_KMod(&draw);
            assert(GetPixel(memory, 504, 0) == RGB(255,0,0));
            assert(GetPixel(memory, 0, 8) == RGB(255,0,0));
            assert(GetPixel(memory, 8, 8) != RGB(255,0,0));
            sat[2] = 2047;
            DrawVDP_Tiles_KMod(&draw);
            assert(GetPixel(memory, 504, 248) == RGB(255,0,0));
            assert(GetPixel(memory, 0, 0) == RGB(255,0,0));
            vdpdebug_select_sprite(-1);
            DrawVDP_Tiles_KMod(&draw);
            assert(GetPixel(memory, 504, 248) != RGB(255,0,0));
        }
        Paused = 1;
        lastTile.x = rcTiles.right - 1;
        lastTile.y = rcTiles.bottom - 1;
        VDPSelectTile_KMod(lastTile);
        assert(tile_KMod == 2047);
        DrawVDP_Tiles_KMod(&draw);
        assert(GetPixel(memory, 511, 255) == RGB(255, 255, 0));
        assert(GetPixel(memory, 504, 248) == RGB(255, 255, 0));
        assert(GetPixel(memory, 510, 254) == RGB(255, 255, 255));
        assert(GetPixel(memory, 507, 251) == RGB(255, 255, 255));
        lastTile.x = rcTiles.left;
        lastTile.y = rcTiles.top;
        VDPSelectTile_KMod(lastTile);
        DrawVDP_Tiles_KMod(&draw);
        assert(GetPixel(memory, 0, 0) == RGB(255, 255, 0));
        assert(GetPixel(memory, 510, 254) == RGB(255, 255, 255));
        {
            HWND tilesWindow = GetDlgItem(hVDP, IDC_VDP_TILES);
            char label[32];
            MSG key = {0};
            assert(SendMessage(tilesWindow, WM_GETDLGCODE, 0, 0) & DLGC_WANTARROWS);
            SendMessage(tilesWindow, WM_KEYDOWN, VK_LEFT, 0);
            SendMessage(tilesWindow, WM_KEYDOWN, VK_UP, 0);
            assert(tile_KMod == 0);
            key.hwnd = tilesWindow;
            key.message = WM_KEYDOWN;
            key.wParam = VK_RIGHT;
            assert(vdpdebug_isMessage(&key));
            assert(tile_KMod == 1);
            SendMessage(tilesWindow, WM_KEYDOWN, VK_DOWN, 0);
            assert(tile_KMod == 65);
            GetDlgItemText(hVDP, IDC_VDP_TILE_ZOOM_ADR, label, sizeof(label));
            assert(!strcmp(label, "Tile 0x0041"));
            DrawVDP_Tiles_KMod(&draw);
            assert(GetPixel(memory, 8, 8) == RGB(255, 255, 0));
            assert(GetPixel(memory, 0, 0) != RGB(255, 255, 0));
            SendMessage(tilesWindow, WM_KEYDOWN, VK_UP, 0);
            SendMessage(tilesWindow, WM_KEYDOWN, VK_LEFT, 0);
            assert(tile_KMod == 0);
            lastTile.x = rcTiles.right - 1;
            lastTile.y = rcTiles.bottom - 1;
            VDPSelectTile_KMod(lastTile);
            SendMessage(tilesWindow, WM_KEYDOWN, VK_RIGHT, 0);
            SendMessage(tilesWindow, WM_KEYDOWN, VK_DOWN, 0);
            assert(tile_KMod == 2047);
            Paused = 0;
            SendMessage(tilesWindow, WM_KEYDOWN, VK_LEFT, 0);
            assert(tile_KMod == 2046);
            Paused = 1;
        }
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(hVDP, dc);
    }
    assert(GetWindowLong(hVDP, GWL_STYLE) & WS_THICKFRAME);
    WritePrivateProfileString("DebugWindows", "VDPRect", "50,50,358,656", config);
    vdpdebug_restore_window(config);
    GetClientRect(hVDP, &actual);
    assert(actual.right >= rcTiles.right);
    assert(!IsWindowVisible(hVDP));
    SendMessage(hVDP, WM_GETMINMAXINFO, 0, (LPARAM)&limits);
    assert(limits.ptMinTrackSize.x > 0 && limits.ptMinTrackSize.y > 0);
    SetWindowPos(hVDP, NULL, 50, 50, limits.ptMinTrackSize.x + 60,
        limits.ptMinTrackSize.y + 20, SWP_NOACTIVATE | SWP_NOZORDER);
    GetWindowRect(hVDP, &expected);
    OpenedWindow_KMod[DMODE_VDP - 1] = TRUE;
    vdpdebug_show(TRUE);
    vdpdebug_save_window(config);
    vdpdebug_destroy();
    assert(!hVDP);
    vdpdebug_create(GetModuleHandle(NULL), NULL);
    vdpdebug_restore_window(config);
    GetWindowRect(hVDP, &actual);
    assert(EqualRect(&expected, &actual));
    assert(IsWindowVisible(hVDP) && OpenedWindow_KMod[DMODE_VDP - 1]);
    vdpdebug_reset();
    GetWindowRect(hVDP, &actual);
    assert(EqualRect(&expected, &actual));
    SendMessage(hVDP, WM_CLOSE, 0, 0);
    vdpdebug_save_window(config);
    vdpdebug_destroy();
    vdpdebug_create(GetModuleHandle(NULL), NULL);
    vdpdebug_restore_window(config);
    GetWindowRect(hVDP, &actual);
    assert(EqualRect(&expected, &actual));
    assert(!IsWindowVisible(hVDP) && !OpenedWindow_KMod[DMODE_VDP - 1]);
    vdpdebug_show(TRUE);
    GetWindowRect(hVDP, &actual);
    assert(EqualRect(&expected, &actual));
    vdpdebug_destroy();
    assert(!PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
    assert(DeleteFile(config));
    puts("PASS: all 2048 VDP tiles, selection, resize, position/size and visibility persistence, close/reopen");
    return 0;
}
