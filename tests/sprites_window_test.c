/* Build the Release solution first, then use an x86 VS developer prompt:
 * cl /nologo /TC tests\sprites_window_test.c /Fo"%TEMP%\sprites_window_test.obj" /Fe"%TEMP%\sprites_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib comdlg32.lib comctl32.lib
 * "%TEMP%\sprites_window_test.exe"
 */
#include <assert.h>
#include "../src/Gens/kmod/sprites.c"

unsigned char VRam[64000], CRam[256];
UCHAR ActivePal, OpenedWindow_KMod[WIN_NUMBER];
int Paused = 1, CRam_Flag;
struct Reg_VDP_Type VDP_Reg;
void FrameStep_KMod(void) { }
int CopyToClipboard(int type, unsigned char *buffer, size_t length, BOOL clear) { return 0; }
COLORREF vdpdebug_getColor(unsigned int pal, unsigned int color) { return 0; }
void vdpdebug_drawTile(HDC dc, unsigned short tile, WORD x, WORD y, UCHAR pal, UCHAR zoom) { }
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
    assert(GetWindowLong(hSprites, GWL_STYLE) & WS_THICKFRAME);
    sprites_restore_window(config);
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
    assert(ListView_GetItemCount(hSpriteList) == 80);
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
    puts("PASS: Sprites resize, position/size and visibility persistence, close/reopen");
    return 0;
}
