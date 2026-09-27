#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "../gens.h"
#include "../resource.h"
#include "../Mem_M68k.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../vdp_32X.h"
#include "common.h"
#include "window_geometry.h"
#include "fps_32x.h"

static HWND fpsWindow;

static void SetTextIfChanged(int id, const char *text)
{
    char previous[2048];
    GetDlgItemText(fpsWindow, id, previous, sizeof(previous));
    if (strcmp(previous, text)) SetDlgItemText(fpsWindow, id, text);
}

/* Count actual displayed-bank changes, including immediate VBlank writes. */
static DWORD sampleStart;
static unsigned swappedFrames, displayedFB;

static void ResetSample(DWORD now)
{
    sampleStart = now;
    swappedFrames = 0;
    displayedFB = _32X_VDP.State & 1;
}

void fps32x_framebuffer_changed(void)
{
    unsigned currentFB = _32X_VDP.State & 1;
    /* Bit 16 is the requested bank; bit 0 is the actual displayed bank.
       Mapping-only updates (e.g. FM changes) must not count as frames. */
    if (currentFB != displayedFB && fpsWindow && IsWindowVisible(fpsWindow) &&
        (_32X_Started || CD_32X_Active) && !Paused && !Debug)
        ++swappedFrames;
    displayedFB = currentFB;
}

static void RefreshFPSAt(DWORD now)
{
    char text[512];
    DWORD elapsed = now - sampleStart;
    if (!_32X_Started && !CD_32X_Active)
    {
        SetTextIfChanged(IDC_32XFPS_SUMMARY, "32X inactive - FPS: --");
        ResetSample(now);
    }
    else if (Paused || Debug)
    {
        SetTextIfChanged(IDC_32XFPS_SUMMARY, "Paused - FPS: --");
        ResetSample(now);
    }
    else if (elapsed >= 500)
    {
        sprintf(text, "Frame rate: %.1f FPS\r\nDisplayed buffer: FB%u\r\nVideo rate: %d Hz (%s)",
            swappedFrames * 1000.0 / elapsed, displayedFB,
            CPU_Mode ? 50 : 60, CPU_Mode ? "PAL" : "NTSC");
        SetTextIfChanged(IDC_32XFPS_SUMMARY, text);
        ResetSample(now);
    }
}

static void RefreshFPS(void) { RefreshFPSAt(GetTickCount()); }

static INT_PTR CALLBACK FPSProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        fpsWindow = hwnd;
        HandleWindow_KMod[DMODE_32_FPS - 1] = hwnd;
        ResetSample(GetTickCount());
        SetTimer(hwnd, 1, 250, NULL);
        RefreshFPS();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) RefreshFPS();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) != IDCANCEL) return FALSE;
        /* Escape uses the same persistent hide behavior as the close button. */
    case WM_CLOSE:
        CloseWindow_KMod(DMODE_32_FPS);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        fpsWindow = NULL;
        HandleWindow_KMod[DMODE_32_FPS - 1] = NULL;
        OpenedWindow_KMod[DMODE_32_FPS - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void fps32x_create(HINSTANCE instance, HWND parent)
{
    fpsWindow = CreateDialog(instance, MAKEINTRESOURCE(IDD_DEBUG32X_FPS), parent, FPSProc);
}
void fps32x_show(BOOL visible)
{
    if (visible) fps32x_reset();
    ShowWindow(fpsWindow, visible ? SW_SHOW : SW_HIDE);
}
void fps32x_reset(void)
{
    ResetSample(GetTickCount());
    if (fpsWindow)
    {
        SetTextIfChanged(IDC_32XFPS_SUMMARY, "Measuring FPS...");
        RefreshFPS();
    }
}
void fps32x_destroy(void) { if (fpsWindow) DestroyWindow(fpsWindow); }
void fps32x_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "32XFPSOpen",
        OpenedWindow_KMod[DMODE_32_FPS - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(fpsWindow, "32XFPSRect", config);
}
void fps32x_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "32XFPSOpen", 0, config) != 0;
    GetWindowRect(fpsWindow, &rect);
    DebugWindow_RestoreGeometry(fpsWindow, "32XFPSRect", config);
    /* Restore position while retaining the current fixed dialog size. */
    SetWindowPos(fpsWindow, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_32_FPS - 1] = visible && fpsWindow != NULL;
    fps32x_show(visible);
}
