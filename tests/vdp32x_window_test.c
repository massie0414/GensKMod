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
    RECT saved, restored, initial, before, after;
    MINMAXINFO limits = {0};
    int width = max(180, GetSystemMetrics(SM_CXMINTRACK));
    int height = max(120, GetSystemMetrics(SM_CYMINTRACK));
    MSG msg;
    assert(GetTempPath(sizeof(dir),dir));
    assert(GetTempFileName(dir,"32x",0,config));
    vdp32x_create(GetModuleHandle(NULL),NULL);
    vdp32x_restore_window(config);
    assert(!IsWindowVisible(h32X_VDP));
    assert(GetWindowLong(h32X_VDP,GWL_STYLE)&WS_THICKFRAME);
    GetWindowRect(h32X_VDP,&initial);
    GetWindowRect(GetDlgItem(h32X_VDP,IDC_32XVDP_TILES),&before);
    MapWindowPoints(NULL,h32X_VDP,(POINT*)&before,2);
    SendMessage(h32X_VDP,WM_GETMINMAXINFO,0,(LPARAM)&limits);
    assert(limits.ptMinTrackSize.x<=width && limits.ptMinTrackSize.y<=height);
    SetWindowPos(h32X_VDP,NULL,20,20,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
    GetWindowRect(h32X_VDP,&saved);
    assert(saved.right-saved.left==width && saved.bottom-saved.top==height);
    assert(width<initial.right-initial.left && height<initial.bottom-initial.top);
    GetWindowRect(GetDlgItem(h32X_VDP,IDC_32XVDP_TILES),&after);
    MapWindowPoints(NULL,h32X_VDP,(POINT*)&after,2);
    assert(EqualRect(&before,&after));
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
    SetWindowPos(h32X_VDP,NULL,0,0,initial.right-initial.left+80,initial.bottom-initial.top+20,
                 SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    GetWindowRect(GetDlgItem(h32X_VDP,IDC_32XVDP_TILES),&after);
    MapWindowPoints(NULL,h32X_VDP,(POINT*)&after,2);
    assert(EqualRect(&before,&after));
    vdp32x_destroy();
    assert(!PeekMessage(&msg,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
    DeleteFile(config);
    puts("PASS: shrink with clipped content, restore small size, expand, open/closed persistence, safe destruction");
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
    assert(!strcmp(text, "Data: --\r\nIndex: --\r\nR: --\r\nG: --\r\nB: --\r\nPriority: --"));
    for (index = 0; index < 256; ++index)
    {
        SendMessage(palette, WM_MOUSEMOVE, 0,
            MAKELPARAM((index % 8) * (rect.right / 8), (index / 8) * (rect.bottom / 32)));
        sprintf(expected, "Data: 0x%04X\r\nIndex: %u (0x%02X)\r\nR: %u\r\nG: %u\r\nB: %u\r\nPriority: %u",
            (unsigned)_32X_VDP_CRam[index], index, index, _32X_VDP_CRam[index] & 31, (_32X_VDP_CRam[index] >> 5) & 31,
            (_32X_VDP_CRam[index] >> 10) & 31, (_32X_VDP_CRam[index] >> 15) & 1);
        GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
        assert(!strcmp(text, expected));
    }
    _32X_VDP_CRam[255] = 0xFC1F;
    Update32X_VDP_KMod();
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
    assert(!strcmp(text, "Data: 0xFC1F\r\nIndex: 255 (0xFF)\r\nR: 31\r\nG: 0\r\nB: 31\r\nPriority: 1"));
    SendMessage(palette, WM_MOUSEMOVE, 0, MAKELPARAM(-1, -1));
    assert(paletteIndex == 255);
    puts("PASS: fixed palette readout, all 256 hover cells, live color and bounds");
}
static void select_view(int id)
{
    CheckRadioButton(h32X_VDP,IDC_32XVDP_FB0,IDC_32XVDP_FB2,id);
}
static void framebuffer_hover(void)
{
    WORD *fb0 = (WORD *)_32X_VDP_Ram, *fb1 = fb0 + 65536;
    int index;
    unsigned color, offset;
    char text[128];
    vdp32x_create(GetModuleHandle(NULL), NULL);
    _32X_Started = 1; VDP_Num_Vis_Lines = 224;
    fb0[0] = fb1[0] = 256;
    fb0[256] = 0x0102; fb1[256] = 0x0304;
    _32X_VDP.Mode = 1;
    SendMessage(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES), WM_MOUSEMOVE, 0, MAKELPARAM(0, 0));
    assert(paletteIndex == 1);
    GetDlgItemText(h32X_VDP, IDC_32XVDP_ADDRESS, text, sizeof(text));
    assert(!strcmp(text, "FB0 +0x00200"));
    SendMessage(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES2), WM_MOUSEMOVE, 0, MAKELPARAM(1, 0));
    assert(paletteIndex == 4);
    GetDlgItemText(h32X_VDP, IDC_32XVDP_ADDRESS, text, sizeof(text));
    assert(!strcmp(text, "FB1 +0x00201"));
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
    assert(strstr(text, "\r\nX: 1   Y: 0"));
    fb1[256] = 0x0305;
    Update32X_VDP_KMod(); assert(paletteIndex == 5);
    _32X_VDP.Mode = 0x10001;
    assert(Sample32XPixel(0, 0, 0, &index, &color, &offset) && index == 2);
    assert(offset == 0x201);
    _32X_VDP.Mode = 3;
    fb0[256] = 0x0201; fb0[257] = 0xff02;
    assert(Sample32XPixel(0, 2, 0, &index, &color, &offset) && index == 1);
    assert(offset == 0x200);
    assert(Sample32XPixel(0, 3, 0, &index, &color, &offset) && index == 2);
    assert(offset == 0x202);
    _32X_VDP.Mode = 2; fb0[256] = 0xFC1F;
    assert(Sample32XPixel(0, 1, 0, &index, &color, &offset) && offset == 0x202);
    SendMessage(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES), WM_MOUSEMOVE, 0, MAKELPARAM(0, 0));
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
    assert(!strcmp(text, "Data: 0xFC1F\r\nIndex: --\r\nR: 31\r\nG: 0\r\nB: 31\r\nPriority: 1\r\nX: 0   Y: 0"));
    assert(!Sample32XPixel(0, 320, 0, &index, &color, &offset));
    assert(!Sample32XPixel(0, -1, 0, &index, &color, &offset));
    assert(!Sample32XPixel(0, 0, 224, &index, &color, &offset));
    fb0[0] = 65535;
    assert(!Sample32XPixel(0, 1, 0, &index, &color, &offset));
    _32X_VDP.Mode = 0;
    assert(!Sample32XPixel(0, 0, 0, &index, &color, &offset));
    select_view(IDC_32XVDP_FB1);
    assert(Sample32XPixel(0, 0, 0, &index, &color, &offset) && index == -1 && color == 0xFC1F);
    assert(offset == 0x200);
    select_view(IDC_32XVDP_FB2); fb0[65535] = 31;
    assert(Sample32XPixel(0, 0, 0, &index, &color, &offset) && color == 31);
    assert(offset == 0x1fffe);
    SendMessage(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES), WM_MOUSEMOVE, 0, MAKELPARAM(1, 0));
    GetDlgItemText(h32X_VDP, IDC_32XVDP_ADDRESS, text, sizeof(text));
    assert(!strcmp(text, "Address: --"));
    SendMessage(GetDlgItem(h32X_VDP, IDC_32XVDP_PAL), WM_MOUSEMOVE, 0, MAKELPARAM(0, 0));
    assert(hoverBank == -1 && paletteIndex == 0);
    GetDlgItemText(h32X_VDP, IDC_32XVDP_ADDRESS, text, sizeof(text));
    assert(!strcmp(text, "Address: --"));
    GetDlgItemText(h32X_VDP, IDC_32XVDP_PALINFO, text, sizeof(text));
    assert(!strstr(text, "X:"));
    vdp32x_destroy();
    puts("PASS: framebuffer hover, both banks, live updates, shift, RLE, direct/raw modes and bounds");
}
static WNDPROC original_line_table_proc;
static unsigned line_table_reads, line_table_writes;
static LRESULT CALLBACK count_line_table_messages(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    if (message == LVM_GETITEMTEXTA) ++line_table_reads;
    if (message == LVM_SETITEMTEXTA) ++line_table_writes;
    return CallWindowProc(original_line_table_proc, hwnd, message, wp, lp);
}

static void line_table_inspection(void)
{
    WORD *fb0 = (WORD *)_32X_VDP_Ram;
    WORD *fb1 = (WORD *)(_32X_VDP_Ram + 0x20000);
    HWND list;
    char text[512];
    vdp32x_create(GetModuleHandle(NULL), NULL);
    list = GetDlgItem(h32X_VDP, IDC_32XVDP_LINETABLE);
    assert(list && ListView_GetItemCount(list) == 256);
    fb0[0] = 0x110; fb0[1] = 0x1d0;
    fb1[0] = 0xffff; fb1[1] = 0x100;
    fb0[255] = 0x1234;
    Update32X_VDP_KMod();
    ListView_GetItemText(list, 1, 1, text, sizeof(text)); assert(!strcmp(text, "+0x00002"));
    ListView_GetItemText(list, 0, 2, text, sizeof(text)); assert(!strcmp(text, "0x0110"));
    ListView_GetItemText(list, 0, 3, text, sizeof(text)); assert(!strcmp(text, "+0x00220"));
    ListView_GetItemText(list, 1, 4, text, sizeof(text)); assert(!strcmp(text, "+192"));
    ListView_GetItemText(list, 0, 6, text, sizeof(text)); assert(!strcmp(text, "+0x1FFFE"));
    ListView_GetItemText(list, 1, 7, text, sizeof(text)); assert(!strcmp(text, "-65279"));
    ListView_GetItemText(list, 255, 2, text, sizeof(text)); assert(!strcmp(text, "0x1234"));
    SendMessage(GetDlgItem(h32X_VDP, IDC_32XVDP_TILES2), WM_MOUSEMOVE, 0, MAKELPARAM(2, 1));
    assert(ListView_GetNextItem(list, -1, LVNI_SELECTED) == 1);
    fb1[1] = 0x3456; Update32X_VDP_KMod();
    GetDlgItemText(h32X_VDP, IDC_32XVDP_LINEINFO, text, sizeof(text));
    assert(strstr(text, "FB1: [0x00002] = 0x3456 -> FB1 +0x068AC"));
    ListView_SetItemState(list, 255, LVIS_SELECTED, LVIS_SELECTED);
    GetDlgItemText(h32X_VDP, IDC_32XVDP_LINEINFO, text, sizeof(text));
    assert(strstr(text, "Y=255 (inactive)"));
    original_line_table_proc = (WNDPROC)SetWindowLongPtr(list, GWLP_WNDPROC,
        (LONG_PTR)count_line_table_messages);
    line_table_reads = line_table_writes = 0;
    Update32X_VDP_KMod();
    assert(!line_table_reads && !line_table_writes);
    ++fb0[0]; Update32X_VDP_KMod();
    assert(!line_table_reads && line_table_writes == 3);
    ListView_GetItemText(list, 1, 4, text, sizeof(text)); assert(!strcmp(text, "+191"));
    line_table_reads = line_table_writes = 0;
    ++fb1[17]; Update32X_VDP_KMod();
    assert(!line_table_reads && line_table_writes == 4);
    line_table_reads = line_table_writes = 0;
    ++fb0[255]; Update32X_VDP_KMod();
    assert(!line_table_reads && line_table_writes == 3);
    SetWindowLongPtr(list, GWLP_WNDPROC, (LONG_PTR)original_line_table_proc);
    vdp32x_destroy();
    /* Recreating the widget must populate every cell even with unchanged RAM. */
    vdp32x_create(GetModuleHandle(NULL), NULL);
    list = GetDlgItem(h32X_VDP, IDC_32XVDP_LINETABLE);
    ListView_GetItemText(list, 255, 2, text, sizeof(text)); assert(!strcmp(text, "0x1235"));
    ListView_GetItemText(list, 1, 4, text, sizeof(text)); assert(!strcmp(text, "+191"));
    vdp32x_destroy();
    puts("PASS: unchanged line table sends no cell messages; updates both banks, next-row deltas, last row and recreation");
    puts("PASS: 256 line entries, byte/word units, both banks, signed deltas, live detail and hover selection");
}
static void palette_paint(void)
{
    BITMAPINFO bmi = {0};
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bitmap; HGDIOBJ old;
    DWORD *pixels, handles;
    DRAWITEMSTRUCT item = {0};
    unsigned index, x, y, pass;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 144; bmi.bmiHeader.biHeight = -384;
    bmi.bmiHeader.biPlanes = 1; bmi.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    assert(bitmap); old = SelectObject(dc, bitmap);
    item.hDC = dc; SetRect(&item.rcItem, 0, 0, 144, 384);
    for (index = 0; index < 256; ++index)
        _32X_VDP_CRam[index] = (WORD)(((index * 131) & 0x7fff) | ((index & 1) << 15));
    for (pass = 0; pass < 10; ++pass) Draw32XPal_KMod(&item);
    GdiFlush();
    handles = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (pass = 0; pass < 100; ++pass) Draw32XPal_KMod(&item);
    GdiFlush();
    assert(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == handles);
    for (index = 0; index < 256; ++index) {
        unsigned value = _32X_VDP_CRam[index], white = 0;
        DWORD color = ((value & 31) << 19) | (((value >> 5) & 31) << 11) | (((value >> 10) & 31) << 3);
        for (y = 0; y < 12; ++y) for (x = 0; x < 18; ++x) {
            DWORD pixel = pixels[((index / 8) * 12 + y) * 144 + (index % 8) * 18 + x] & 0xffffff;
            assert(pixel == color || ((value & 0x8000) && pixel == 0xffffff));
            if (pixel == 0xffffff) ++white;
        }
        assert((white != 0) == ((value & 0x8000) != 0));
    }
    SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
    puts("PASS: all 256 palette colors, priority stipple, repeated paint without GDI leaks");
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
    InitCommonControls();
    palette_paint();
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
    framebuffer_hover();
    persistence();line_table_inspection();return 0;
}
