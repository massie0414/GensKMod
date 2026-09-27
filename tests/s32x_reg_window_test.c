/* Build from an x86 VS developer prompt after building Release:
 * cl /nologo /O2 /MT /TC tests\s32x_reg_window_test.c /Fo"src\Gens\bin\s32x_reg_window_test.obj" /Fe"src\Gens\bin\s32x_reg_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib comctl32.lib
 */
#include <assert.h>
#include <string.h>
#include "../src/Gens/kmod/s32x_reg.c"
#include "../src/Gens/kmod/common.c"

unsigned char _32X_Comm[16], _32X_ADEN, _32X_RES, _32X_FM, _32X_RV;
unsigned char _32X_MINT, _32X_HIC;
unsigned int _32X_DREQ_ST, _32X_DREQ_SRC, _32X_DREQ_DST, _32X_DREQ_LEN;
unsigned short _32X_FIFO_A[4], _32X_FIFO_B[4];
unsigned int _32X_FIFO_Block, _32X_FIFO_Read, _32X_FIFO_Write;
unsigned int PWM_Mode, PWM_Cycle_Tmp, PWM_RP_L, PWM_WP_L, PWM_RP_R, PWM_WP_R;
unsigned char PWM_FULL_TAB[16] = {0x40,0,0,0x80,0x80,0x40,0,0,0,0x80,0x40,0,0,0,0x80,0x40};
struct VDP_32X_Type _32X_VDP;
int Bank_SH2, CD_32X_Active;

/* Exercise the actual common open/close dispatch; other windows are unused. */
#define STUB_SHOW(name) void name(BOOL visible) { assert(0); }
STUB_SHOW(message_show)
STUB_SHOW(watchers_show)
STUB_SHOW(layers_show)
STUB_SHOW(m68kdebug_show)
STUB_SHOW(z80debug_show)
STUB_SHOW(vdpdebug_show)
STUB_SHOW(vdpreg_show)
STUB_SHOW(sprites_show)
STUB_SHOW(ym2612_show)
STUB_SHOW(psg_show)
STUB_SHOW(s68kdebug_show)
STUB_SHOW(cdcdebug_show)
STUB_SHOW(cdgfx_show)
STUB_SHOW(cdreg_show)
STUB_SHOW(mSH2_show)
STUB_SHOW(sSH2_show)
STUB_SHOW(vdp32x_show)
void planes_show(int plane, BOOL visible) { assert(0); }
void Update_KMod(void) { s32xreg_update(); }

static void expect_cell(int row, int column, const char *expected)
{
    char actual[64];
    ListView_GetItemText(h32XRegList, row, column, actual, sizeof(actual));
    assert(!strcmp(actual, expected));
}

static void check_resize(void)
{
    RECT before, after, client;
    MINMAXINFO limits = {0};
    assert(GetWindowLong(h32X_Reg, GWL_STYLE) & WS_THICKFRAME);
    assert(GetWindowLong(h32X_Reg, GWL_STYLE) & WS_MAXIMIZEBOX);
    GetWindowRect(h32XRegList, &before);
    SetWindowPos(h32X_Reg, NULL, 0, 0,
        minimum32XRegSize.cx + 320, minimum32XRegSize.cy + 200,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    GetWindowRect(h32XRegList, &after);
    assert(after.right - after.left == before.right - before.left + 320);
    assert(after.bottom - after.top == before.bottom - before.top + 200);
    MapWindowPoints(NULL, h32X_Reg, (POINT *)&after, 2);
    GetClientRect(h32X_Reg, &client);
    assert(after.left == client.right - after.right);
    assert(after.top == client.bottom - after.bottom);
    SendMessage(h32X_Reg, WM_GETMINMAXINFO, 0, (LPARAM)&limits);
    assert(limits.ptMinTrackSize.x == minimum32XRegSize.cx);
    assert(limits.ptMinTrackSize.y == minimum32XRegSize.cy);
    SetWindowPos(h32X_Reg, NULL, 0, 0,
        minimum32XRegSize.cx, minimum32XRegSize.cy,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    GetWindowRect(h32XRegList, &after);
    assert(after.right - after.left == before.right - before.left);
    assert(after.bottom - after.top == before.bottom - before.top);
    puts("PASS: resizable frame, list growth/shrink, margins, minimum size");
}

int main(void)
{
    MSG msg;
    WORD value;
    int i;
    InitCommonControls();
    s32xreg_create(GetModuleHandle(NULL), NULL);
    assert(IsWindow(h32X_Reg) && IsWindow(h32XRegList));
    assert(ListView_GetItemCount(h32XRegList) == 34);
    assert(Header_GetItemCount(ListView_GetHeader(h32XRegList)) == 6);
    assert(!IsWindowVisible(h32X_Reg));
    check_resize();
    _32X_Comm[0] = 0xAB; _32X_Comm[1] = 0xCD;
    _32X_DREQ_ST = 4; _32X_DREQ_LEN = 8;
    _32X_FIFO_Read = 3; _32X_FIFO_Write = 2;
    _32X_FIFO_B[3] = 0x1234; _32X_FIFO_A[3] = 0x5678;
    _32X_VDP.State = 0xA002;
    _32X_DREQ_SRC = 0x12345678; _32X_DREQ_DST = 0xABCDEF01;
    _32X_ADEN = 1; _32X_RES = 2; _32X_FM = 0x80; _32X_MINT = 0x0F;
    CD_32X_Active = 1;
    OpenWindow_KMod(DMODE_32_REG);
    assert(IsWindowVisible(h32X_Reg));
    expect_cell(0, 2, "0x8083"); expect_cell(0, 3, "0x830F");
    expect_cell(4, 3, "0x1234"); expect_cell(5, 3, "0x5678");
    expect_cell(6, 3, "0xABCD"); expect_cell(7, 3, "0xEF01");
    expect_cell(9, 2, "--"); expect_cell(9, 3, "0x1234");
    expect_cell(10, 3, "--"); expect_cell(15, 2, "0xABCD");
    expect_cell(15, 3, "0xABCD"); expect_cell(33, 3, "0xA002");
    for (i = 0; i < 100; ++i) s32xreg_update();
    assert(_32X_FIFO_Read == 3 && _32X_FIFO_Write == 2 && _32X_FIFO_Block == 0);
    assert(_32X_DREQ_ST == 4 && _32X_DREQ_LEN == 8 && _32X_VDP.State == 0xA002);
    _32X_FIFO_Block = 8;
    assert(Peek32XRegister(0x12, TRUE, &value) && value == 0x5678);
    _32X_DREQ_ST = 0x4004;
    assert(Peek32XRegister(0x12, TRUE, &value) && value == 0);
    PWM_WP_L = 3;
    assert(Peek32XRegister(0x34, TRUE, &value) && value == 0x8000);
    assert(Peek32XRegister(0x36, TRUE, &value) && value == 0x4000);
    _32X_Comm[1] = 0xEF;
    s32xreg_update(); expect_cell(15, 3, "0xABEF");
    for (i = 0; i < 3; ++i) s32xreg_reset();
    assert(ListView_GetItemCount(h32XRegList) == 34);
    assert(Header_GetItemCount(ListView_GetHeader(h32XRegList)) == 6);
    CloseWindow_KMod(DMODE_32_REG);
    assert(!IsWindowVisible(h32X_Reg));
    OpenWindow_KMod(DMODE_32_REG);
    assert(IsWindowVisible(h32X_Reg));
    SendMessage(h32X_Reg, WM_CLOSE, 0, 0);
    assert(!IsWindowVisible(h32X_Reg) && !OpenedWindow_KMod[DMODE_32_REG - 1]);
    s32xreg_destroy();
    assert(!h32X_Reg && !h32XRegList && !HandleWindow_KMod[DMODE_32_REG - 1]);
    assert(!PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
    s32xreg_create(GetModuleHandle(NULL), NULL);
    assert(ListView_GetItemCount(h32XRegList) == 34);
    s32xreg_destroy();
    {
        char dir[MAX_PATH], config[MAX_PATH];
        RECT saved, restored;
        assert(GetTempPath(sizeof(dir), dir));
        assert(GetTempFileName(dir, "reg", 0, config));
        s32xreg_create(GetModuleHandle(NULL), NULL);
        s32xreg_restore_window(config);
        assert(!IsWindowVisible(h32X_Reg));
        SetWindowPos(h32X_Reg, NULL, 40, 50,
            minimum32XRegSize.cx + 120, minimum32XRegSize.cy + 80,
            SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(h32X_Reg, &saved);
        OpenWindow_KMod(DMODE_32_REG);
        s32xreg_save_window(config);
        s32xreg_destroy();
        s32xreg_create(GetModuleHandle(NULL), NULL);
        s32xreg_restore_window(config);
        GetWindowRect(h32X_Reg, &restored);
        assert(EqualRect(&saved, &restored));
        assert(IsWindowVisible(h32X_Reg) && OpenedWindow_KMod[DMODE_32_REG - 1]);
        SendMessage(h32X_Reg, WM_CLOSE, 0, 0);
        s32xreg_save_window(config);
        s32xreg_destroy();
        s32xreg_create(GetModuleHandle(NULL), NULL);
        s32xreg_restore_window(config);
        GetWindowRect(h32X_Reg, &restored);
        assert(EqualRect(&saved, &restored));
        assert(!IsWindowVisible(h32X_Reg) && !OpenedWindow_KMod[DMODE_32_REG - 1]);
        s32xreg_destroy();
        DeleteFile(config);
        puts("PASS: saved geometry and open/closed state survive window recreation");
    }
    puts("PASS: menu dispatch, initialization, values, side-effect-free refresh, reset, close/reopen, destruction");
    return 0;
}
