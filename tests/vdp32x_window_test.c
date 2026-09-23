/* From an x86 VS developer prompt, after building Release:
 * cl /nologo /O2 /MT /TC tests\vdp32x_window_test.c /Fo"src\Gens\bin\vdp32x-tests\vdp32x_window_test.obj" /Fe"src\Gens\bin\vdp32x-tests\vdp32x_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib comdlg32.lib comctl32.lib
 * src\Gens\bin\vdp32x-tests\vdp32x_window_test.exe
 * Create the vdp32x-tests output directory first.
 * Uses the real drawing code and a hidden dialog with synthetic framebuffer data.
 */
#include <assert.h>
#include <string.h>
#include "../src/Gens/kmod/vdp_32x.c"
unsigned char _32X_VDP_Ram[0x40000];
unsigned short _32X_VDP_CRam[256], _32X_VDP_CRam_Ajusted[256];
struct VDP_32X_Type _32X_VDP;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
char Rom_Name[512];
void Put_Info(const char *text, int duration) { }
void CloseWindow_KMod(UCHAR mode) { OpenedWindow_KMod[mode-1]=0; vdp32x_show(FALSE); }

/* Retained per-pixel algorithm for timing and output comparison. */
static void reference(LPDRAWITEMSTRUCT d)
{
    HDC dc=CreateCompatibleDC(d->hDC);
    HBITMAP bm=CreateCompatibleBitmap(d->hDC,320,240);
    HGDIOBJ old=SelectObject(dc,bm);
    const WORD *fb=(const WORD *)_32X_VDP_Ram;
    unsigned x,y;
    for(y=0;y<240;y++)for(x=0;x<320;x++) {
        unsigned a=256+y*320+x;
        WORD c=a<65536?fb[a]:0;
        SetPixelV(dc,x,y,RGB((c&31)*8,((c>>5)&31)*8,((c>>10)&31)*8));
    }
    BitBlt(d->hDC,d->rcItem.left,d->rcItem.top,320,240,dc,0,0,SRCCOPY);
    SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
}
static double timing(void (*paint)(LPDRAWITEMSTRUCT),DRAWITEMSTRUCT *d)
{
    LARGE_INTEGER f,a,z;
    int n;
    paint(d);GdiFlush();QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    for(n=0;n<20;n++){paint(d);GdiFlush();}
    QueryPerformanceCounter(&z);
    return 1000.0*(z.QuadPart-a.QuadPart)/f.QuadPart/20;
}
static void persistence(void)
{
    char dir[MAX_PATH], config[MAX_PATH];
    RECT saved, restored;
    MSG msg;
    assert(GetTempPath(sizeof(dir),dir));
    assert(GetTempFileName(dir,"32x",0,config));
    vdp32x_create(GetModuleHandle(NULL),NULL);
    vdp32x_restore_window(config);
    assert(!IsWindowVisible(h32X_VDP));
    assert(GetWindowLong(h32X_VDP,GWL_STYLE)&WS_THICKFRAME);
    SetWindowPos(h32X_VDP,NULL,20,20,minimum32XVDPSize.cx+80,minimum32XVDPSize.cy+20,SWP_NOZORDER|SWP_NOACTIVATE);
    GetWindowRect(h32X_VDP,&saved);
    OpenedWindow_KMod[DMODE_32_VDP-1]=TRUE;
    vdp32x_save_window(config);
    vdp32x_destroy(); assert(!h32X_VDP);
    vdp32x_create(GetModuleHandle(NULL),NULL);
    vdp32x_restore_window(config);
    GetWindowRect(h32X_VDP,&restored);
    assert(EqualRect(&saved,&restored));
    assert(IsWindowVisible(h32X_VDP) && OpenedWindow_KMod[DMODE_32_VDP-1]);
    SendMessage(h32X_VDP,WM_CLOSE,0,0);
    vdp32x_save_window(config); vdp32x_destroy();
    vdp32x_create(GetModuleHandle(NULL),NULL); vdp32x_restore_window(config);
    GetWindowRect(h32X_VDP,&restored);
    assert(EqualRect(&saved,&restored));
    assert(!IsWindowVisible(h32X_VDP) && !OpenedWindow_KMod[DMODE_32_VDP-1]);
    vdp32x_destroy();
    assert(!PeekMessage(&msg,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
    DeleteFile(config);
    puts("PASS: open/closed persistence, position/size, safe window destruction");
}
int main(void)
{
    BITMAPINFO bmi={0};
    HDC dc=CreateCompatibleDC(NULL);
    HBITMAP bm; HGDIOBJ old;
    DWORD *output;
    DWORD expected[320*240];
    DRAWITEMSTRUCT d={0};
    WORD *fb=(WORD *)_32X_VDP_Ram;
    unsigned i,x,y;
    DWORD handles;
    double slow,fast;
    bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth=340;bmi.bmiHeader.biHeight=-260;
    bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;
    bm=CreateDIBSection(dc,&bmi,DIB_RGB_COLORS,(void **)&output,NULL,0);
    assert(dc && bm); old=SelectObject(dc,bm);
    vdp32x_create(GetModuleHandle(NULL),NULL);assert(h32X_VDP);
    assert(!IsWindowVisible(h32X_VDP));
    for(i=0;i<65536;i++){fb[i]=(WORD)i;fb[65536+i]=0x7c00;}
    d.hDC=dc;d.rcItem.left=7;d.rcItem.top=9;d.rcItem.right=327;d.rcItem.bottom=249;
    reference(&d);GdiFlush();
    for(y=0;y<240;y++)memcpy(expected+y*320,output+(y+9)*340+7,320*4);
    memset(output,0xcd,340*260*4);
    Draw32XVDPRaw_KMod(&d);GdiFlush();
    for(y=0;y<240;y++)for(x=0;x<320;x++)
        assert((output[(y+9)*340+x+7]&0xffffff)==(expected[y*320+x]&0xffffff));
    assert(output[0]==0xcdcdcdcd); /* respects rectangle origin */
    assert(output[(239+9)*340+7]==0); /* raw tail is outside selected bank */
    /* Explicit banks and Current, with visibly distinct color channels. */
    CheckRadioButton(h32X_VDP,IDC_32XVDP_FB0,IDC_32XVDP_FB2,IDC_32XVDP_FB2);
    assert(Selected32XFB_KMod()==1);
    Draw32XVDPRaw_KMod(&d);GdiFlush();assert(output[9*340+7]==0x000000f8);
    CheckRadioButton(h32X_VDP,IDC_32XVDP_FB0,IDC_32XVDP_FB2,IDC_32XVDP_FB1);
    _32X_VDP.State=1;assert(Selected32XFB_KMod()==0);
    CheckRadioButton(h32X_VDP,IDC_32XVDP_FB0,IDC_32XVDP_FB2,IDC_32XVDP_FB0);
    assert(Selected32XFB_KMod()==1);_32X_VDP.State=0;assert(Selected32XFB_KMod()==0);
    fb[0]=0xffff;fb[65535]=0x001f;
    fb[1]=1000;fb[1000]=0x03e0;
    Draw32XVDP_KMod(&d);GdiFlush();
    assert(output[9*340+7]==0x00f80000 && output[9*340+8]==0);
    assert(output[10*340+7]==0x0000f800);
    /* Smaller controls must show the top-left, not the bottom of the DIB. */
    memset(output,0xcd,340*260*4);d.rcItem.right=17;d.rcItem.bottom=11;
    Draw32XVDP_KMod(&d);GdiFlush();
    assert(output[9*340+7]==0x00f80000 && output[10*340+7]==0x0000f800);
    assert(output[11*340+7]==0xcdcdcdcd && output[9*340+17]==0xcdcdcdcd);
    d.rcItem.right=327;d.rcItem.bottom=249;
    Draw32XVDPRaw_KMod(&d);GdiFlush();handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    for(i=0;i<200;i++){Draw32XVDPRaw_KMod(&d);Draw32XVDP_KMod(&d);}
    GdiFlush();assert(handles==GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS));
    slow=timing(reference,&d);fast=timing(Draw32XVDPRaw_KMod,&d);
    printf("PASS: pixel equivalence, origin/crop, bank selection, boundaries, GDI lifetime\n");
    printf("Per image: per-pixel %.3f ms, batched %.3f ms, %.1fx speedup\n",slow,fast,slow/fast);
    SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
    vdp32x_destroy();
    persistence();
    return 0;
}
