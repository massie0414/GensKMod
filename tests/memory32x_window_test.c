#include <assert.h>
#include "../src/Gens/kmod/memory_32x.c"
int _32X_Started, CD_32X_Active;
unsigned char _32X_FM;
struct VDP_32X_Type _32X_VDP;
unsigned char _32X_Ram[256*1024], _32X_Rom[4*1024*1024];
unsigned char _32X_MSH2_Rom[2048], _32X_SSH2_Rom[1024], _32X_VDP_Ram[256*1024];
unsigned short _32X_VDP_CRam[256], _32X_VDP_CRam_Ajusted[256], _32X_Palette_16B[65536];
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode) { OpenedWindow_KMod[mode-1]=0; memory32x_show(FALSE); }
static void SetRegion(unsigned i) {
    SendDlgItemMessage(memoryWindow,IDC_MEMORY_REGION,CB_SETCURSEL,i,0);
    SendMessage(memoryWindow,WM_COMMAND,MAKEWPARAM(IDC_MEMORY_REGION,CBN_SELCHANGE),0);
}
int main(void) {
    unsigned n; char text[32];
    INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&ic);
    memory32x_create(GetModuleHandle(NULL),NULL); assert(memoryWindow);
    {
        RECT before, after, statusBefore, statusAfter;
        MINMAXINFO limits = {0};
        GetWindowRect(grid, &before);
        GetWindowRect(GetDlgItem(memoryWindow, IDC_MEMORY_STATUS), &statusBefore);
        SendMessage(memoryWindow, WM_GETMINMAXINFO, 0, (LPARAM)&limits);
        assert(limits.ptMinTrackSize.x == fixedWidth && limits.ptMaxTrackSize.x == fixedWidth);
        GetWindowRect(memoryWindow, &after);
        SetWindowPos(memoryWindow, NULL, 0, 0, fixedWidth, after.bottom-after.top+100,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(grid, &after);
        GetWindowRect(GetDlgItem(memoryWindow, IDC_MEMORY_STATUS), &statusAfter);
        assert(after.bottom-after.top == before.bottom-before.top+100);
        assert(after.right-after.left == before.right-before.left);
        assert(statusAfter.top == statusBefore.top+100);
    }
    assert(SendDlgItemMessage(memoryWindow,IDC_MEMORY_REGION,CB_GETCOUNT,0,0)==9);
    assert(SendDlgItemMessage(memoryWindow,IDC_MEMORY_REGION,CB_FINDSTRINGEXACT,-1,(LPARAM)"Framebuffer 0")==CB_ERR);
    assert(SendDlgItemMessage(memoryWindow,IDC_MEMORY_REGION,CB_FINDSTRINGEXACT,-1,(LPARAM)"Framebuffer 1")==CB_ERR);
    assert(!IsWindowEnabled(GetDlgItem(memoryWindow,IDC_MEMORY_WRITE)));
    assert(ParseHex("0x0603FFFF",&n) && n==0x0603ffff);
    assert(!ParseHex("",&n) && !ParseHex("100000000",&n) && !ParseHex("-1",&n) && !ParseHex("GG",&n));
    _32X_Started=1; Refresh(); assert(IsWindowEnabled(GetDlgItem(memoryWindow,IDC_MEMORY_WRITE)));
    SelectByte(0x3ffff); SetDlgItemText(memoryWindow,IDC_MEMORY_VALUE,"AB"); WriteByte();
    assert(_32X_Ram[0x3ffff]==0xab && _32X_Ram[0x3fffe]==0);
    SetRegion(1); SetDlgItemText(memoryWindow,IDC_MEMORY_VALUE,"FF"); WriteByte(); assert(_32X_Rom[0]==0);
    assert(ListView_GetItemCount(grid)==0x40000);
    _32X_Rom[0]=0x12; _32X_Rom[1]=0x34;
    ListView_GetItemText(grid,0,1,text,sizeof(text)); assert(!strcmp(text,"12"));
    ListView_GetItemText(grid,0,2,text,sizeof(text)); assert(!strcmp(text,"34"));
    _32X_Started=0; CD_32X_Active=1;
    SetRegion(REGION_PALETTE); assert(ListView_GetItemCount(grid)==32);
    _32X_VDP_CRam[255]=0x1234;
    _32X_Palette_16B[0xab34]=0x5678; _32X_Palette_16B[0xabcd]=0x9abc;
    SelectByte(510); assert(ReadByte(510)==0x12 && ReadByte(511)==0x34);
    SetDlgItemText(memoryWindow,IDC_MEMORY_VALUE,"AB"); WriteByte();
    assert(_32X_VDP_CRam[255]==0xab34 && _32X_VDP_CRam_Ajusted[255]==0x5678);
    SelectByte(511); SetDlgItemText(memoryWindow,IDC_MEMORY_VALUE,"CD"); WriteByte();
    assert(_32X_VDP_CRam[255]==0xabcd && _32X_VDP_CRam_Ajusted[255]==0x9abc);
    CD_32X_Active=0; SetDlgItemText(memoryWindow,IDC_MEMORY_VALUE,"00"); WriteByte();
    assert(_32X_VDP_CRam[255]==0xabcd); CD_32X_Active=1;
    _32X_FM=1; _32X_VDP.State=0;
    _32X_VDP_Ram[0x20000+511]=0xcd;
    SetRegion(REGION_LINE0); assert(ListView_GetItemCount(grid)==32);
    assert(ReadByte(510)==0xcd);
    ListView_GetItemText(grid,0,0,text,sizeof(text)); assert(!strcmp(text,"04000000"));
    SetRegion(REGION_LINE1); assert(ListView_GetItemCount(grid)==32);
    assert(ReadByte(510)==0xcd);
    ListView_GetItemText(grid,0,0,text,sizeof(text)); assert(!strcmp(text,"04020000"));
    ListView_GetItemText(grid,31,0,text,sizeof(text)); assert(!strcmp(text,"040201F0"));
    SelectByte(511); GetDlgItemText(memoryWindow,IDC_MEMORY_ADDRESS,text,sizeof(text));
    assert(!strcmp(text,"040201FF"));
    _32X_VDP.State=1; assert(ReadByte(510)==0);
    _32X_FM=1; _32X_VDP.State=0;
    _32X_VDP_Ram[1]=0x12; _32X_VDP_Ram[0x20001]=0x34;
    SetRegion(REGION_SH2_FB); assert(ReadByte(0)==0x34);
    assert(ListView_GetItemCount(grid)==8192);
    ListView_GetItemText(grid,0,0,text,sizeof(text)); assert(!strcmp(text,"04000000"));
    SetRegion(REGION_SH2_OVERWRITE); assert(ReadByte(0)==0x34);
    ListView_GetItemText(grid,0,0,text,sizeof(text)); assert(!strcmp(text,"04020000"));
    ListView_GetItemText(grid,8191,0,text,sizeof(text)); assert(!strcmp(text,"0403FFF0"));
    _32X_VDP.State=0x10000; assert(ReadByte(0)==0x34); /* pending swap */
    _32X_VDP.State=1; Refresh(); assert(ReadByte(0)==0x12);
    SetRegion(REGION_SH2_FB); assert(ReadByte(0)==0x12);
    SetDlgItemText(memoryWindow,IDC_MEMORY_VALUE,"FF"); WriteByte();
    assert(_32X_VDP_Ram[1]==0x12); /* address views are inspection only */
    _32X_FM=0; Refresh(); assert(!Readable());
    ListView_GetItemText(grid,0,1,text,sizeof(text)); assert(!strcmp(text,"--"));
    SetRegion(REGION_LINE0); assert(!Readable());
    _32X_FM=1; assert(Readable() && ReadByte(0)==0x12);
    SetRegion(0); SetDlgItemText(memoryWindow,IDC_MEMORY_ADDRESS,"06001234");
    SendMessage(memoryWindow,WM_COMMAND,IDC_MEMORY_GOTO,0); assert(selected==0x1234);
    memory32x_show(TRUE); SendMessage(memoryWindow,WM_COMMAND,IDCANCEL,0); assert(!IsWindowVisible(memoryWindow));
    DestroyWindow(memoryWindow); assert(!HandleWindow_KMod[DMODE_32_MEMORY-1]);
    puts("PASS: SH2 address aliases, actual/pending swaps, access gating, palette byte order/cache, line table aliases/bounds, hex validation, virtual grid, SDRAM boundary writes, read-only ROM, framebuffer byte order, inactive/CD32X, Go, close and cleanup");
    return 0;
}
