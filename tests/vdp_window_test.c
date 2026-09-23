/* Build the Release solution first, then use an x86 VS developer prompt:
 * cl /nologo /TC tests\vdp_window_test.c /Fo"%TEMP%\vdp_window_test.obj" /Fe"%TEMP%\vdp_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib comdlg32.lib
 * "%TEMP%\vdp_window_test.exe"
 */
#include <assert.h>
#include "../src/Gens/kmod/vdp.c"

unsigned char VRam[64000], CRam[256];
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
    assert(GetWindowLong(hVDP, GWL_STYLE) & WS_THICKFRAME);
    vdpdebug_restore_window(config);
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
    puts("PASS: VDP resize, position/size and visibility persistence, close/reopen");
    return 0;
}
