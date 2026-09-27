#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <math.h>
#include "../pwm.h"
#include "../G_dsound.h"
#include <string.h>
#include "../gens.h"
#include "../resource.h"
#include "../Mem_M68k.h"
#include "../G_main.h"
#include "../vdp_io.h"
#include "../vdp_32X.h"
#include "common.h"
#include "window_geometry.h"
#include "sound_32x.h"

static HWND soundWindow;

static void SetTextIfChanged(int id, const char *text)
{
    char previous[2048];
    GetDlgItemText(soundWindow, id, previous, sizeof(previous));
    if (strcmp(previous, text)) SetDlgItemText(soundWindow, id, text);
}

/* Samples match the signed contribution in PWM_Update, before other sources
   are mixed. Weight by output sample count; remove DC for a silent held DAC. */
static double sum[2], squares[2], count;
static void ClearSample(void) { sum[0]=sum[1]=squares[0]=squares[1]=count=0; }
void sound32x_sample(int length)
{
    unsigned raw[2]; int c;
    if (!soundWindow || !IsWindowVisible(soundWindow) || length <= 0 ||
        (! _32X_Started && !CD_32X_Active) || Paused || Debug || !PWM_Enable || !Sound_Enable) return;
    raw[0]=PWM_Out_L; raw[1]=PWM_Out_R;
    for(c=0;c<2;++c) {
        double value=(int)((raw[c] << 5) & 0xffff) - 0x4000;
        sum[c]+=value*length; squares[c]+=value*value*length;
    }
    count+=length;
}
static void RefreshSound(void)
{
    double level[2]={0,0}; int c; char text[128];
    const char *status="PWM stereo output";
    if (!_32X_Started && !CD_32X_Active) status="32X inactive";
    else if (Paused || Debug) status="Paused";
    else if (!PWM_Enable || !Sound_Enable) status="PWM / sound disabled";
    else if (count > 0) {
        for(c=0;c<2;++c) {
            double variance=squares[c]/count-(sum[c]/count)*(sum[c]/count);
            level[c]=variance > 0 ? sqrt(variance)*100.0/16384.0 : 0;
            if(level[c]>100) level[c]=100;
        }
    }
    SetTextIfChanged(IDC_32XSOUND_STATUS,status);
    sprintf(text,"L: %.1f%%                 R: %.1f%%",level[0],level[1]);
    SetTextIfChanged(IDC_32XSOUND_LEVELS,text);
    SendDlgItemMessage(soundWindow,IDC_32XSOUND_LEFT,PBM_SETPOS,(int)(level[0]*10),0);
    SendDlgItemMessage(soundWindow,IDC_32XSOUND_RIGHT,PBM_SETPOS,(int)(level[1]*10),0);
    ClearSample();
}

static INT_PTR CALLBACK SoundProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        soundWindow = hwnd;
        HandleWindow_KMod[DMODE_32_SOUND - 1] = hwnd;
        ClearSample();
        SendDlgItemMessage(hwnd, IDC_32XSOUND_LEFT, PBM_SETRANGE32, 0, 1000);
        SendDlgItemMessage(hwnd, IDC_32XSOUND_RIGHT, PBM_SETRANGE32, 0, 1000);
        SetTimer(hwnd, 1, 50, NULL);
        RefreshSound();
        return TRUE;
    case WM_TIMER:
        if (wParam == 1 && IsWindowVisible(hwnd)) RefreshSound();
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) != IDCANCEL) return FALSE;
        /* Escape uses the same persistent hide behavior as the close button. */
    case WM_CLOSE:
        CloseWindow_KMod(DMODE_32_SOUND);
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd, 1);
        soundWindow = NULL;
        HandleWindow_KMod[DMODE_32_SOUND - 1] = NULL;
        OpenedWindow_KMod[DMODE_32_SOUND - 1] = FALSE;
        return TRUE;
    }
    return FALSE;
}

void sound32x_create(HINSTANCE instance, HWND parent)
{
    soundWindow = CreateDialog(instance, MAKEINTRESOURCE(IDD_DEBUG32X_SOUND), parent, SoundProc);
}
void sound32x_show(BOOL visible)
{
    if (visible) sound32x_reset();
    ShowWindow(soundWindow, visible ? SW_SHOW : SW_HIDE);
}
void sound32x_reset(void)
{
    ClearSample();
    if (soundWindow)
    {
        RefreshSound();
    }
}
void sound32x_destroy(void) { if (soundWindow) DestroyWindow(soundWindow); }
void sound32x_save_window(const char *config)
{
    WritePrivateProfileString("DebugWindows", "32XSoundOpen",
        OpenedWindow_KMod[DMODE_32_SOUND - 1] ? "1" : "0", config);
    DebugWindow_SaveGeometry(soundWindow, "32XSoundRect", config);
}
void sound32x_restore_window(const char *config)
{
    RECT rect;
    BOOL visible = GetPrivateProfileInt("DebugWindows", "32XSoundOpen", 0, config) != 0;
    GetWindowRect(soundWindow, &rect);
    DebugWindow_RestoreGeometry(soundWindow, "32XSoundRect", config);
    /* Restore position while retaining the current fixed dialog size. */
    SetWindowPos(soundWindow, NULL, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    OpenedWindow_KMod[DMODE_32_SOUND - 1] = visible && soundWindow != NULL;
    sound32x_show(visible);
}
