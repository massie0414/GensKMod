#include <assert.h>
#include "../src/Gens/kmod/fps_32x.c"
int _32X_Started, CD_32X_Active, Paused, Debug, CPU_Mode;
struct VDP_32X_Type _32X_VDP;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode) {
    OpenedWindow_KMod[mode - 1] = 0; fps32x_show(FALSE);
}
static void expect(const char *part) {
    char text[512];
    GetDlgItemText(fpsWindow, IDC_32XFPS_SUMMARY, text, sizeof(text));
    assert(strstr(text, part));
}
int main(void) {
    int i;
    char dir[MAX_PATH], config[MAX_PATH];
    fps32x_create(GetModuleHandle(NULL), NULL); assert(fpsWindow);
    expect("inactive");
    _32X_Started = 1; fps32x_show(TRUE); ResetSample(1000);
    /* Repeated mapping updates and pending requests are not switches. */
    for (i = 0; i < 60; ++i) fps32x_framebuffer_changed();
    _32X_VDP.State |= 0x10000; fps32x_framebuffer_changed();
    assert(!swappedFrames);
    /* Two VBlanks per switch: 30 FPS over one second of 60 Hz video. */
    for (i = 0; i < 60; ++i) {
        if (i % 2 == 0) _32X_VDP.State ^= 1;
        fps32x_framebuffer_changed();
    }
    RefreshFPSAt(2000); expect("Frame rate: 30.0"); expect("60 Hz (NTSC)");
    RefreshFPSAt(2500); expect("Frame rate: 0.0");
    /* Count both transitions even when they occur in one video frame. */
    _32X_VDP.State ^= 1; fps32x_framebuffer_changed();
    _32X_VDP.State ^= 1; fps32x_framebuffer_changed();
    assert(swappedFrames == 2);
    RefreshFPSAt(3000); expect("Frame rate: 4.0");
    Paused = 1; RefreshFPSAt(3250); expect("Paused");
    _32X_VDP.State ^= 1; fps32x_framebuffer_changed(); assert(!swappedFrames);
    Paused = 0; CPU_Mode = 1; _32X_Started = 0; CD_32X_Active = 1;
    for (i = 0; i < 25; ++i) { _32X_VDP.State ^= 1; fps32x_framebuffer_changed(); }
    RefreshFPSAt(3750); expect("Frame rate: 50.0"); expect("50 Hz (PAL)");
    ResetSample(0xFFFFFF00UL);
    for (i = 0; i < 30; ++i) { _32X_VDP.State ^= 1; fps32x_framebuffer_changed(); }
    RefreshFPSAt(244); expect("Frame rate: 60.0");
    fps32x_show(FALSE); _32X_VDP.State ^= 1; fps32x_framebuffer_changed(); assert(!swappedFrames);
    fps32x_show(TRUE); expect("Measuring");
    fps32x_framebuffer_changed(); assert(!swappedFrames);
    GetTempPath(sizeof(dir), dir); GetTempFileName(dir, "fps", 0, config);
    OpenedWindow_KMod[DMODE_32_FPS - 1] = 1;
    fps32x_save_window(config); fps32x_destroy();
    fps32x_create(GetModuleHandle(NULL), NULL); fps32x_restore_window(config);
    assert(IsWindowVisible(fpsWindow));
    CD_32X_Active = 0; SendMessage(fpsWindow, WM_TIMER, 1, 0); expect("inactive");
    SendMessage(fpsWindow, WM_COMMAND, IDCANCEL, 0);
    assert(!IsWindowVisible(fpsWindow) && !OpenedWindow_KMod[DMODE_32_FPS - 1]);
    fps32x_destroy(); assert(!HandleWindow_KMod[DMODE_32_FPS - 1]);
    DeleteFile(config);
    puts("PASS: FB switches, pending/same-bank ignored, multiple switches, stopped, pause, PAL, CD32X, tick wrap, hide, persistence, cleanup");
    return 0;
}
