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
unsigned short _32X_Palette_16B[65536];
int Mode_555, VDP_Num_Vis_Lines, _32X_Started, CD_32X_Active;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
char Rom_Name[512];
void Put_Info(const char *text, int duration) { }
void CloseWindow_KMod(UCHAR mode) { OpenedWindow_KMod[mode-1]=0; vdp32x_show(FALSE); }

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
static DWORD decoded[320*240];
static void palette_hover(void)
{
    HWND palette = GetDlgItem(h32X_VDP, IDC_32XVDP_PAL);
    RECT rect;
    char text[128], expected[128];
    unsigned index;
    GetClientRect(palette, &rect);
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
    assert(!strcmp(text, "Index: --\r\nR: --\r\nG: --\r\nB: --\r\nPriority: --"));
    for (index = 0; index < 256; ++index)
    {
        SendMessage(palette, WM_MOUSEMOVE, 0,
            MAKELPARAM((index % 8) * (rect.right / 8), (index / 8) * (rect.bottom / 32)));
        sprintf(expected, "Index: %u (0x%02X)\r\nR: %u\r\nG: %u\r\nB: %u\r\nPriority: %u",
            index, index, _32X_VDP_CRam[index] & 31, (_32X_VDP_CRam[index] >> 5) & 31,
            (_32X_VDP_CRam[index] >> 10) & 31, (_32X_VDP_CRam[index] >> 15) & 1);
        GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
        assert(!strcmp(text, expected));
    }
    _32X_VDP_CRam[255] = 0xFC1F;
    Update32X_VDP_KMod();
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
    assert(!strcmp(text, "Index: 255 (0xFF)\r\nR: 31\r\nG: 0\r\nB: 31\r\nPriority: 1"));
    SendMessage(palette, WM_MOUSEMOVE, 0, MAKELPARAM(-1, -1));
    assert(paletteIndex == 255);
    puts("PASS: fixed palette readout, all 256 hover cells, live color and bounds");
}
static void select_view(int id)
{
    CheckRadioButton(h32X_VDP,IDC_32XVDP_FB0,IDC_32XVDP_FB2,id);
}
int main(void)
{
    unsigned i;
    WORD *fb0=(WORD *)_32X_VDP_Ram, *fb1=fb0+65536;
    BITMAPINFO bmi={0};
    HDC dc=CreateCompatibleDC(NULL);
    HBITMAP bm; HGDIOBJ old;
    DWORD *output, handles;
    DRAWITEMSTRUCT d={0};
    LARGE_INTEGER f,a,z;
    for(i=0;i<65536;i++)
        _32X_Palette_16B[i]=(WORD)(((i&31)<<11)|(((i>>5)&31)<<6)|((i>>10)&31));
    _32X_VDP_CRam[1]=31; _32X_VDP_CRam[2]=31<<5; _32X_VDP_CRam[3]=31<<10;
    _32X_Started=1;VDP_Num_Vis_Lines=224;
    fb0[0]=fb1[0]=256;
    fb0[256]=0x0103;fb1[256]=0x0301;
    _32X_VDP.Mode=1;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0xff0000 && decoded[1]==0x0000ff);
    Decode32X_KMod(1,decoded);assert(decoded[0]==0x0000ff && decoded[1]==0xff0000);
    _32X_Started=0;CD_32X_Active=1;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0xff0000 && decoded[1]==0x0000ff);
    Decode32X_KMod(1,decoded);assert(decoded[0]==0x0000ff && decoded[1]==0xff0000);
    CD_32X_Active=0;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0 && decoded[1]==0);
    Decode32X_KMod(1,decoded);assert(decoded[0]==0 && decoded[1]==0);
    _32X_Started=1;
    _32X_VDP.Mode=0x10001;fb0[257]=0x0101;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0x0000ff && decoded[1]==0xff0000);
    _32X_VDP.Mode=2;fb0[256]=0x001f;fb0[257]=0xfc00;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0xff0000 && decoded[1]==0x0000ff);
    _32X_VDP.Mode=3;fb0[256]=0x0201;fb0[257]=0xff03;fb0[258]=0xff01;
    Decode32X_KMod(0,decoded);
    assert(decoded[0]==0xff0000 && decoded[2]==0xff0000 && decoded[3]==0x0000ff);
    assert(decoded[258]==0x0000ff && decoded[259]==0xff0000 && decoded[319]==0xff0000);
    fb0[0]=65535;fb0[65535]=0x0001;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0xff0000 && decoded[1]==0);
    _32X_VDP.Mode=2;fb0[65535]=31;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0xff0000 && decoded[1]==0);
    fb0[224]=65535;Decode32X_KMod(0,decoded);assert(decoded[224*320]==0);
    VDP_Num_Vis_Lines=240;Decode32X_KMod(0,decoded);assert(decoded[224*320]==0xff0000);
    Mode_555=1;_32X_Palette_16B[31]=0x7c00;
    Decode32X_KMod(0,decoded);assert(decoded[0]==0xff0000);Mode_555=0;_32X_Palette_16B[31]=0xf800;
    _32X_VDP.Mode=0;Decode32X_KMod(0,decoded);assert(decoded[0]==0);
    _32X_VDP.Mode=2;_32X_Started=0;Decode32X_KMod(0,decoded);assert(decoded[0]==0);_32X_Started=1;
    puts("PASS: independent FB0/FB1, packed order/shift, direct/priority, RLE, boundaries, heights, blanking, RGB555");

    bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bmi.bmiHeader.biWidth=340;bmi.bmiHeader.biHeight=-260;
    bmi.bmiHeader.biPlanes=1;bmi.bmiHeader.biBitCount=32;
    bm=CreateDIBSection(dc,&bmi,DIB_RGB_COLORS,(void **)&output,NULL,0);assert(bm);old=SelectObject(dc,bm);
    vdp32x_create(GetModuleHandle(NULL),NULL);assert(h32X_VDP);
    palette_hover();
    d.hDC=dc;d.rcItem.left=7;d.rcItem.top=9;d.rcItem.right=327;d.rcItem.bottom=249;
    fb1[256]=0x7c00;
    Draw32XVDPRaw_KMod(&d);GdiFlush();assert(output[9*340+7]==0xff0000);
    Draw32XVDP_KMod(&d);GdiFlush();assert(output[9*340+7]==0x0000ff);
    _32X_Started=0;CD_32X_Active=1;
    Draw32XVDPRaw_KMod(&d);GdiFlush();assert(output[9*340+7]==0xff0000);
    Draw32XVDP_KMod(&d);GdiFlush();assert(output[9*340+7]==0x0000ff);
    CD_32X_Active=0;
    Draw32XVDPRaw_KMod(&d);GdiFlush();assert(output[9*340+7]==0);
    Draw32XVDP_KMod(&d);GdiFlush();assert(output[9*340+7]==0);
    _32X_Started=1;
    puts("PASS: CD32X active without cartridge, both panel paints, inactive blanking");
    select_view(IDC_32XVDP_FB1);fb0[256]=31;
    Draw32XVDPRaw_KMod(&d);GdiFlush();assert(output[9*340+7]==0xf80000);
    select_view(IDC_32XVDP_FB2);Draw32XVDP_KMod(&d);GdiFlush();assert(output[9*340+7]==0x0000f8);
    select_view(IDC_32XVDP_FB0);
    memset(output,0xcd,340*260*4);d.rcItem.right=17;d.rcItem.bottom=11;
    Draw32XVDPRaw_KMod(&d);GdiFlush();assert(output[9*340+7]==0xff0000);
    assert(output[9*340+17]==0xcdcdcdcd && output[11*340+7]==0xcdcdcdcd);
    d.rcItem.right=327;d.rcItem.bottom=249;
    for(i=0;i<10;i++){Draw32XVDPRaw_KMod(&d);Draw32XVDP_KMod(&d);GdiFlush();}
    handles=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
    QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    for(i=0;i<200;i++){Draw32XVDPRaw_KMod(&d);Draw32XVDP_KMod(&d);GdiFlush();}
    QueryPerformanceCounter(&z);
    assert(handles==GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS));
    printf("PASS: two-panel paint, raw modes, crop, GDI lifetime; %.3f ms per pair\n",1000.0*(z.QuadPart-a.QuadPart)/f.QuadPart/200);
    vdp32x_destroy();SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
    persistence();return 0;
}
