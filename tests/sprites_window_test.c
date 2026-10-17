/* Build the Release solution first, then use an x86 VS developer prompt:
 * cl /nologo /TC tests\sprites_window_test.c /Fo"%TEMP%\sprites_window_test.obj" /Fe"%TEMP%\sprites_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib comdlg32.lib comctl32.lib
 * "%TEMP%\sprites_window_test.exe"
 */
#include <assert.h>
#include "../src/Gens/kmod/sprites.c"

unsigned char VRam[65536], CRam[256];
UCHAR ActivePal, OpenedWindow_KMod[WIN_NUMBER];
int Paused = 1, CRam_Flag;
struct Reg_VDP_Type VDP_Reg;
void FrameStep_KMod(void) { }
int CopyToClipboard(int type, unsigned char *buffer, size_t length, BOOL clear) { return 0; }
COLORREF vdpdebug_getColor(unsigned int pal, unsigned int color) { return 0; }
static int selectedVDPTileSprite = -1;
void vdpdebug_select_sprite(int index) { selectedVDPTileSprite = index; }
static int selectedScreenSprite = -1;
void planes_select_sprite(int index) { selectedScreenSprite = index; }
static int drawnTiles;
void vdpdebug_drawTile(HDC dc, unsigned short tile, WORD x, WORD y, UCHAR pal, UCHAR zoom)
{
    assert(tile < 2048);
    drawnTiles++;
    PatBlt(dc, x, y, 8, 8, WHITENESS);
}
char Rom_Name[512];
void Put_Info(const char *message, int duration) { }
void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    sprites_show(FALSE);
}

int main(void)
{
    char temp[MAX_PATH], config[MAX_PATH];
    RECT expected, actual;
    MINMAXINFO limits = {0};
    MSG msg;
    InitCommonControls();
    assert(GetTempPath(sizeof(temp), temp));
    assert(GetTempFileName(temp, "vdp", 0, config));
    sprites_create(GetModuleHandle(NULL), NULL);
    assert(hSprites);
    assert(!GetDlgItem(hSprites, IDC_SPRITES_PREVIEW));
    assert(GetDlgItem(hSprites, IDC_SPRITES_PREVIEW2));
    {
        RECT gallery, client;
        POINT pt = {383, 519};
        HDC dc = GetDC(hSprites), memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, 384, 520);
        HBITMAP old = SelectObject(memory, bitmap);
        DRAWITEMSTRUCT draw = {0};
        assert(hSpriteGallery);
        GetClientRect(hSpriteGallery, &gallery);
        assert(gallery.right == 384 && gallery.bottom == 520);
        GetWindowRect(hSpriteGallery, &gallery);
        MapWindowPoints(NULL, hSprites, (LPPOINT)&gallery, 2);
        GetClientRect(hSprites, &client);
        assert(gallery.right <= client.right && gallery.bottom <= client.bottom);
        /* Every entry, including number 79, is rendered and selectable. */
        draw.hDC = memory;
        SetRect(&draw.rcItem, 0, 0, 384, 520);
        DrawSpriteGallery_KMod(&draw);
        assert(drawnTiles == 0);
        assert(GetPixel(memory, 7 * 48 + 20, 9 * 52 + 19) == GetSysColor(COLOR_3DFACE));
        {
            int i;
            for (i = 0; i < 80; i++)
                ((unsigned short *)VRam)[i * 4 + 3] = 128;
        }
        DrawSpriteGallery_KMod(&draw);
        assert(drawnTiles == 80);
        assert(GetPixel(memory, 7 * 48 + 20, 9 * 52 + 19) == RGB(255,255,255));
        {
            int i;
            for (i = 0; i < 80; i++)
            {
                ((unsigned short *)VRam)[i * 4 + 1] = 0x0F00;
                ((unsigned short *)VRam)[i * 4 + 2] = 2047;
            }
            drawnTiles = 0;
            DrawSpriteGallery_KMod(&draw);
            assert(drawnTiles == 80 * 16);
        }
        SelectSpriteGallery_KMod(pt);
        assert(SelectedSprite_KMod() == 79);
        assert(selectedScreenSprite == 79);
        pt.x = 384;
        SelectSpriteGallery_KMod(pt);
        assert(SelectedSprite_KMod() == 79);
        assert(selectedScreenSprite == 79);
        sprites_show(TRUE);
        ValidateRect(hSpriteGallery, NULL);
        UpdateSprites_KMod();
        ValidateRect(hSpriteGallery, NULL);
        UpdateSprites_KMod();
        assert(GetUpdateRect(hSpriteGallery, NULL, FALSE));
        {
            char text[32];
            ListView_GetItemText(hSpriteList2, 39, 0, text, sizeof(text));
            assert(!strcmp(text, "79"));
            ListView_GetItemText(hSpriteList2, 39, 6, text, sizeof(text));
            assert(!strcmp(text, "0x07FF"));
            ListView_SetItemState(hSpriteList, 5, LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
            assert(SelectedSprite_KMod() == 5);
            assert(ListView_GetNextItem(hSpriteList2, -1, LVNI_SELECTED) == -1);
            ListView_SetItemState(hSpriteList2, 0, LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
            assert(SelectedSprite_KMod() == 40);
            assert(selectedScreenSprite == 40);
            assert(selectedVDPTileSprite == 40);
            assert(ListView_GetNextItem(hSpriteList, -1, LVNI_SELECTED) == -1);
            SetFocus(hSpriteList2);
            SendMessage(hSpriteList2, WM_KEYDOWN, VK_UP, 0);
            assert(SelectedSprite_KMod() == 39);
            assert(GetFocus() == hSpriteList);
            assert(selectedScreenSprite == 39);
            SendMessage(hSpriteList, WM_KEYDOWN, VK_DOWN, 0);
            assert(SelectedSprite_KMod() == 40);
            assert(GetFocus() == hSpriteList2);
            assert(selectedScreenSprite == 40);
            assert(selectedVDPTileSprite == 40);
            SendMessage(hSpriteList2, WM_KEYDOWN, VK_DOWN, 0);
            assert(SelectedSprite_KMod() == 41);
            SendMessage(hSpriteList2, WM_KEYDOWN, VK_UP, 0);
            assert(SelectedSprite_KMod() == 40);
        }
        SelectObject(memory, old);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(hSprites, dc);
    }
    assert(GetWindowLong(hSprites, GWL_STYLE) & WS_THICKFRAME);
    WritePrivateProfileString("DebugWindows", "SpritesRect", "50,50,470,455", config);
    sprites_restore_window(config);
    GetWindowRect(hSprites, &actual);
    assert(actual.right - actual.left >= minimumSpritesSize.cx);
    assert(actual.bottom - actual.top >= minimumSpritesSize.cy);
    assert(!IsWindowVisible(hSprites));
    SendMessage(hSprites, WM_GETMINMAXINFO, 0, (LPARAM)&limits);
    assert(limits.ptMinTrackSize.x > 0 && limits.ptMinTrackSize.y > 0);
    SetWindowPos(hSprites, NULL, 50, 50, limits.ptMinTrackSize.x + 60,
        limits.ptMinTrackSize.y + 20, SWP_NOACTIVATE | SWP_NOZORDER);
    GetWindowRect(hSprites, &expected);
    OpenedWindow_KMod[DMODE_SPRITES - 1] = TRUE;
    sprites_show(TRUE);
    sprites_save_window(config);
    sprites_destroy();
    assert(!hSprites);
    sprites_create(GetModuleHandle(NULL), NULL);
    sprites_restore_window(config);
    GetWindowRect(hSprites, &actual);
    assert(EqualRect(&expected, &actual));
    assert(IsWindowVisible(hSprites) && OpenedWindow_KMod[DMODE_SPRITES - 1]);
    assert(ListView_GetItemCount(hSpriteList) == 40);
    assert(ListView_GetItemCount(hSpriteList2) == 40);
    assert(ListView_GetCountPerPage(hSpriteList) >= 40);
    assert(ListView_GetCountPerPage(hSpriteList2) >= 40);
    assert(!(GetWindowLong(hSpriteList, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)));
    assert(!(GetWindowLong(hSpriteList2, GWL_STYLE) & (WS_VSCROLL | WS_HSCROLL)));
    sprites_reset();
    assert(Header_GetItemCount(ListView_GetHeader(hSpriteList)) == 8);
    GetWindowRect(hSprites, &actual);
    assert(EqualRect(&expected, &actual));
    SendMessage(hSprites, WM_CLOSE, 0, 0);
    sprites_save_window(config);
    sprites_destroy();
    sprites_create(GetModuleHandle(NULL), NULL);
    sprites_restore_window(config);
    GetWindowRect(hSprites, &actual);
    assert(EqualRect(&expected, &actual));
    assert(!IsWindowVisible(hSprites) && !OpenedWindow_KMod[DMODE_SPRITES - 1]);
    sprites_show(TRUE);
    GetWindowRect(hSprites, &actual);
    assert(EqualRect(&expected, &actual));
    sprites_destroy();
    assert(!PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
    assert(DeleteFile(config));
    puts("PASS: all 80 sprite images, tile wrapping, gallery selection and refresh, resize, position/size and visibility persistence, close/reopen");
    return 0;
}
