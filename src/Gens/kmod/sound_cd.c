#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>
#include "../gens.h"
#include "../resource.h"
#include "../G_main.h"
#include "../G_dsound.h"
#include "../vdp_io.h"
#include "../pcm.h"
#include "../cd_sys.h"
#include "window_geometry.h"
#include "common.h"
#include "sound_cd.h"

static HWND windows[2];
static unsigned peaks[2][2];
static unsigned pcmPeaks[8][2];
int soundcd_active(int source)
{
    return windows[source] && IsWindowVisible(windows[source]) &&
        SegaCD_Started && !Paused && !Debug && Sound_Enable &&
        (source ? CDDA_Enable : PCM_Enable);
}
/* Each sample is the source's contribution before mixing with other chips.
   Peak amplitude is relative to signed 16-bit full scale. */
void soundcd_sample(int source, int left, int right)
{
    unsigned l = left < 0 ? 0u - (unsigned)left : (unsigned)left;
    unsigned r = right < 0 ? 0u - (unsigned)right : (unsigned)right;
    if (l > peaks[source][0]) peaks[source][0] = l;
    if (r > peaks[source][1]) peaks[source][1] = r;
}
void soundcd_pcm_sample(int channel, int left, int right)
{
    unsigned l = left < 0 ? 0u-(unsigned)left : (unsigned)left;
    unsigned r = right < 0 ? 0u-(unsigned)right : (unsigned)right;
    if (l > pcmPeaks[channel][0]) pcmPeaks[channel][0] = l;
    if (r > pcmPeaks[channel][1]) pcmPeaks[channel][1] = r;
}
static void RefreshPCM(void)
{
    int channel, active=soundcd_active(0);
    for(channel=0;channel<8;channel++) {
        unsigned l=active ? pcmPeaks[channel][0] : 0;
        unsigned r=active ? pcmPeaks[channel][1] : 0;
        char text[80];
        if(l>32768) l=32768;
        if(r>32768) r=32768;
        SendDlgItemMessage(windows[0],IDC_PCM_CHANNEL_BASE+channel*3,PBM_SETPOS,l*1000/32768,0);
        SendDlgItemMessage(windows[0],IDC_PCM_CHANNEL_BASE+channel*3+1,PBM_SETPOS,r*1000/32768,0);
        sprintf(text,"%.1f%% / %.1f%%",l*100.0/32768,r*100.0/32768);
        SetDlgItemText(windows[0],IDC_PCM_CHANNEL_BASE+channel*3+2,text);
        pcmPeaks[channel][0]=pcmPeaks[channel][1]=0;
    }
}
static void Refresh(int source)
{
    unsigned l=0, r=0; char text[128];
    if (!source) RefreshPCM();
    if (soundcd_active(source)) {
        l=peaks[source][0]; r=peaks[source][1];
        if (l>32768) l=32768;
        if (r>32768) r=32768;
    }
    SendDlgItemMessage(windows[source], IDC_32XSOUND_LEFT, PBM_SETPOS, l*1000/32768, 0);
    SendDlgItemMessage(windows[source], IDC_32XSOUND_RIGHT, PBM_SETPOS, r*1000/32768, 0);
    sprintf(text,"L: %.1f%%                 R: %.1f%%",l*100.0/32768,r*100.0/32768);
    SetDlgItemText(windows[source],IDC_32XSOUND_LEVELS,text);
    SetDlgItemText(windows[source],IDC_32XSOUND_STATUS,
        !SegaCD_Started ? "SegaCD inactive" : Paused || Debug ? "Paused" :
        !Sound_Enable || !(source ? CDDA_Enable : PCM_Enable) ? "Sound disabled" :
        source ? "CDDA stereo output" : "PCM output - 8 channels");
    peaks[source][0]=peaks[source][1]=0;
}
static INT_PTR CALLBACK SoundProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    int source=(int)GetWindowLongPtr(hwnd,GWLP_USERDATA);
    switch(msg) {
    case WM_INITDIALOG:
        source=(int)lp;
        SetWindowLongPtr(hwnd,GWLP_USERDATA,source);
        windows[source]=hwnd;
        HandleWindow_KMod[(source ? DMODE_CD_CDDA : DMODE_CD_PCM)-1]=hwnd;
        SetWindowText(hwnd,source ? "SegaCD - Sound - CDDA" : "SegaCD - Sound - PCM");
        SendDlgItemMessage(hwnd,IDC_32XSOUND_LEFT,PBM_SETRANGE32,0,1000);
        SendDlgItemMessage(hwnd,IDC_32XSOUND_RIGHT,PBM_SETRANGE32,0,1000);
        if(!source) {
            int channel;
            for(channel=0;channel<8;channel++) {
                SendDlgItemMessage(hwnd,IDC_PCM_CHANNEL_BASE+channel*3,PBM_SETRANGE32,0,1000);
                SendDlgItemMessage(hwnd,IDC_PCM_CHANNEL_BASE+channel*3+1,PBM_SETRANGE32,0,1000);
            }
        }
        SetTimer(hwnd,1,50,NULL);
        Refresh(source);
        return TRUE;
    case WM_TIMER:
        if(wp==1 && IsWindowVisible(hwnd)) Refresh(source);
        return TRUE;
    case WM_COMMAND:
        if(LOWORD(wp)!=IDCANCEL) return FALSE;
    case WM_CLOSE:
        CloseWindow_KMod((UCHAR)(source ? DMODE_CD_CDDA : DMODE_CD_PCM));
        return TRUE;
    case WM_DESTROY:
        KillTimer(hwnd,1);
        windows[source]=NULL;
        HandleWindow_KMod[(source ? DMODE_CD_CDDA : DMODE_CD_PCM)-1]=NULL;
        OpenedWindow_KMod[(source ? DMODE_CD_CDDA : DMODE_CD_PCM)-1]=FALSE;
        return TRUE;
    }
    return FALSE;
}
void soundcd_create(HINSTANCE instance, HWND parent)
{
    int i;
    for(i=0;i<2;i++) CreateDialogParam(instance,MAKEINTRESOURCE(i ? IDD_DEBUGCD_SOUND : IDD_DEBUGCD_PCM),parent,SoundProc,i);
}
void soundcd_show(int source, BOOL visible)
{
    peaks[source][0]=peaks[source][1]=0;
    ShowWindow(windows[source],visible ? SW_SHOW : SW_HIDE);
    Refresh(source);
}
void soundcd_reset(void)
{
    int i;
    memset(pcmPeaks,0,sizeof(pcmPeaks));
    for(i=0;i<2;i++) { peaks[i][0]=peaks[i][1]=0; if(windows[i]) Refresh(i); }
}

void soundcd_save_window(const char *config)
{
    int i;
    for (i=0; i<2; i++) {
        WritePrivateProfileString("DebugWindows", i ? "CDSoundCDDAOpen" : "CDSoundPCMOpen",
            OpenedWindow_KMod[(i ? DMODE_CD_CDDA : DMODE_CD_PCM)-1] ? "1" : "0", config);
        DebugWindow_SaveGeometry(windows[i], i ? "CDSoundCDDARect" : "CDSoundPCMRect", config);
    }
}
void soundcd_restore_window(const char *config)
{
    int i;
    for (i=0; i<2; i++) {
        RECT rect;
        BOOL visible=GetPrivateProfileInt("DebugWindows", i ? "CDSoundCDDAOpen" : "CDSoundPCMOpen", 0, config)!=0;
        GetWindowRect(windows[i], &rect);
        DebugWindow_RestoreGeometry(windows[i], i ? "CDSoundCDDARect" : "CDSoundPCMRect", config);
        SetWindowPos(windows[i], NULL, 0, 0, rect.right-rect.left, rect.bottom-rect.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        OpenedWindow_KMod[(i ? DMODE_CD_CDDA : DMODE_CD_PCM)-1]=visible && windows[i]!=NULL;
        soundcd_show(i, visible);
    }
}
