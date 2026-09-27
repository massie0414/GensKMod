#include <assert.h>
#include "../src/Gens/kmod/sound_32x.c"
int _32X_Started, CD_32X_Active, Paused, Debug, CPU_Mode;
unsigned PWM_Out_L, PWM_Out_R, PWM_Enable;
int Sound_Enable;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode) {
    OpenedWindow_KMod[mode - 1] = 0; sound32x_show(FALSE);
}

static int level(int id) { return (int)SendDlgItemMessage(soundWindow,id,PBM_GETPOS,0,0); }
int main(void) {
    INITCOMMONCONTROLSEX ic={sizeof(ic), ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&ic);
    sound32x_create(GetModuleHandle(NULL),NULL); assert(soundWindow);
    _32X_Started=1; PWM_Enable=1; Sound_Enable=1; sound32x_show(TRUE);
    /* Full-scale alternating left, half-scale right, unequal sample weights. */
    PWM_Out_L=0; PWM_Out_R=256; sound32x_sample(100);
    PWM_Out_L=1024; PWM_Out_R=768; sound32x_sample(100);
    RefreshSound();
    assert(level(IDC_32XSOUND_LEFT)==1000); assert(level(IDC_32XSOUND_RIGHT)==500);
    /* A held DAC is DC, not audible volume. */
    PWM_Out_L=1000; sound32x_sample(100); RefreshSound();
    assert(level(IDC_32XSOUND_LEFT)==0);
    _32X_Started=0; CD_32X_Active=1;
    PWM_Out_L=0; sound32x_sample(100); PWM_Out_L=1024; sound32x_sample(300);
    RefreshSound(); assert(level(IDC_32XSOUND_LEFT)==866);
    PWM_Out_L=0; sound32x_sample(100); PWM_Out_L=1024; sound32x_sample(100);
    Paused=1; RefreshSound(); assert(level(IDC_32XSOUND_LEFT)==0); Paused=0;
    PWM_Enable=0; sound32x_sample(100); assert(count==0); PWM_Enable=1;
    Sound_Enable=0; sound32x_sample(100); assert(count==0); Sound_Enable=1;
    sound32x_show(FALSE); sound32x_sample(100); assert(count==0);
    sound32x_show(TRUE); RefreshSound(); assert(level(IDC_32XSOUND_RIGHT)==0);
    SendMessage(soundWindow,WM_COMMAND,IDCANCEL,0); assert(!IsWindowVisible(soundWindow));
    sound32x_destroy(); assert(!HandleWindow_KMod[DMODE_32_SOUND-1]);
    puts("PASS: stereo RMS, DC silence, sample weighting, CD32X, pause, disable, hide and cleanup");
    return 0;
}
