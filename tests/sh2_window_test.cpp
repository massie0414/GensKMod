#include <windows.h>
#include <commctrl.h>
#include <assert.h>
#include <stdio.h>
#include "../src/Gens/resource.h"
extern "C" {
#include "../src/Gens/gens.h"
#include "../src/Gens/kmod/common.h"
}
#include "../src/Gens/kmod/mSH2.h"
#include "../src/Gens/kmod/sSH2.h"
#include "../src/Gens/kmod/sh2_usage_graph.h"
static void check_text_bounds(HWND hwnd, int mode) {
    int ids[3] = {mode == DMODE_32_MSH2 ? IDC_MSH2_STATUS_SR : IDC_SSH2_STATUS_SR,
                  mode == DMODE_32_MSH2 ? IDC_MSH2_STATUS_ADR : IDC_SSH2_STATUS_ADR,
                  mode == DMODE_32_MSH2 ? IDC_MSH2_STATUS_DATA : IDC_SSH2_STATUS_DATA};
    const char *text[3] = {
        "T=0 S=0 Q=0 M=0 I=F SR=FFFF Status=FFFF",
        "R0=FFFFFFFF R1=FFFFFFFF R2=FFFFFFFF R3=FFFFFFFF\nR4=FFFFFFFF R5=FFFFFFFF R6=FFFFFFFF R7=FFFFFFFF\nR8=FFFFFFFF R9=FFFFFFFF RA=FFFFFFFF RB=FFFFFFFF\nRC=FFFFFFFF RD=FFFFFFFF RE=FFFFFFFF RF=FFFFFFFF",
        "GBR=FFFFFFFF VBR=FFFFFFFF PR=FFFFFFFF\nMACH=FFFFFFFF MACL=FFFFFFFF\nIL=FF IV=FF"
    };
    RECT group, client, previous = {0};
    GetWindowRect(GetDlgItem(hwnd,IDC_SH2_USAGE_GROUP),&group);
    GetClientRect(hwnd,&client);
    MapWindowPoints(hwnd,NULL,(POINT*)&client,2);
    for (int i=0;i<3;++i) {
        RECT bounds, required = {0}, overlap;
        HWND control=GetDlgItem(hwnd,ids[i]);
        HDC dc=GetDC(control);
        HGDIOBJ old=SelectObject(dc,(HFONT)SendMessage(control,WM_GETFONT,0,0));
        DrawText(dc,text[i],-1,&required,DT_CALCRECT|DT_LEFT|DT_NOPREFIX);
        SelectObject(dc,old); ReleaseDC(control,dc);
        GetWindowRect(control,&bounds);
        assert(bounds.right-bounds.left>=required.right);
        assert(bounds.bottom-bounds.top>=required.bottom);
        assert(bounds.bottom<=client.bottom);
        assert(!IntersectRect(&overlap,&bounds,&group));
        if(i) assert(bounds.top>=previous.bottom);
        previous=bounds;
    }
}
static void check_graph(HWND hwnd, int mode) {
    int id=mode==DMODE_32_MSH2 ? IDC_MSH2_USAGE : IDC_SSH2_USAGE;
    SH2_CONTEXT *cpu=mode==DMODE_32_MSH2 ? &M_SH2 : &S_SH2;
    HWND control=GetDlgItem(hwnd,id);
    RECT bounds, group, note;
    GetWindowRect(control,&bounds);
    GetWindowRect(GetDlgItem(hwnd,IDC_SH2_USAGE_GROUP),&group);
    GetWindowRect(GetDlgItem(hwnd,IDC_SH2_USAGE_NOTE),&note);
    assert(bounds.left>group.left && bounds.right<group.right);
    assert(bounds.top>group.top && bounds.bottom<=note.top);
    assert(note.bottom<group.bottom);
    assert((GetWindowLong(control,GWL_STYLE)&SS_TYPEMASK)==SS_OWNERDRAW);
    SH2_Usage_ClearHistory(cpu);
    for(int i=0;i<256;++i) {
        cpu->Odometer=1000; cpu->Idle_Cycles=500; SH2_Usage_End_Frame(cpu);
    }
    HDC screen=GetDC(control), dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,300,140);
    HGDIOBJ old=SelectObject(dc,bitmap);
    DRAWITEMSTRUCT item={0};
    item.hwndItem=control; item.hDC=dc; SetRect(&item.rcItem,0,0,300,140);
    SH2Usage_DrawGraph(&item,cpu,1);
    assert(GetPixel(dc,100,30)==GetSysColor(COLOR_WINDOW));
    assert(GetPixel(dc,100,110)==RGB(0,0,255));
    SH2Usage_DrawGraph(&item,cpu,0);
    assert(GetPixel(dc,100,110)==GetSysColor(COLOR_WINDOW));
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(control,screen);
    SH2_Usage_ClearHistory(cpu);
}
static void check(int mode, int list, void (*create)(HINSTANCE, HWND),
                  void (*destroy)(), void (*save)(const char *), void (*restore)(const char *)) {
    char dir[MAX_PATH], config[MAX_PATH];
    RECT initial, saved, actual, before, after;
    MSG msg;
    MINMAXINFO limits = {0};
    assert(GetTempPath(sizeof(dir),dir));
    assert(GetTempFileName(dir,"sh2",0,config));
    create(GetModuleHandle(NULL),NULL);
    HWND hwnd=HandleWindow_KMod[mode-1];
    assert(IsWindow(hwnd));
    restore(config); assert(!IsWindowVisible(hwnd));
    check_graph(hwnd,mode);
    check_text_bounds(hwnd,mode);
    assert(GetWindowLong(hwnd,GWL_STYLE)&WS_THICKFRAME);
    GetWindowRect(hwnd,&initial);
    GetWindowRect(GetDlgItem(hwnd,list),&before);
    SendMessage(hwnd,WM_GETMINMAXINFO,0,(LPARAM)&limits);
    assert(limits.ptMinTrackSize.x==initial.right-initial.left);
    SetWindowPos(hwnd,NULL,40,50,initial.right-initial.left+120,
                 initial.bottom-initial.top+80,SWP_NOZORDER|SWP_NOACTIVATE);
    GetWindowRect(GetDlgItem(hwnd,list),&after);
    check_text_bounds(hwnd,mode);
    assert(after.right-after.left==before.right-before.left+120);
    assert(after.bottom-after.top==before.bottom-before.top+80);
    ShowWindow(hwnd,SW_SHOW); OpenedWindow_KMod[mode-1]=TRUE;
    GetWindowRect(hwnd,&saved); save(config); destroy();
    assert(!HandleWindow_KMod[mode-1]);
    assert(!PeekMessage(&msg,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
    create(GetModuleHandle(NULL),NULL); restore(config);
    hwnd=HandleWindow_KMod[mode-1]; GetWindowRect(hwnd,&actual);
    check_text_bounds(hwnd,mode);
    assert(EqualRect(&saved,&actual));
    assert(IsWindowVisible(hwnd)&&OpenedWindow_KMod[mode-1]);
    SendMessage(hwnd,WM_CLOSE,0,0);
    assert(!IsWindowVisible(hwnd)&&!OpenedWindow_KMod[mode-1]);
    save(config); destroy(); create(GetModuleHandle(NULL),NULL); restore(config);
    hwnd=HandleWindow_KMod[mode-1]; GetWindowRect(hwnd,&actual);
    check_text_bounds(hwnd,mode);
    assert(EqualRect(&saved,&actual)); assert(!IsWindowVisible(hwnd));
    destroy(); DeleteFile(config);
}
int main() {
    InitCommonControls();
    check(DMODE_32_MSH2,IDC_MSH2_DISAM,mSH2_create,mSH2_destroy,mSH2_save_window,mSH2_restore_window);
    check(DMODE_32_SSH2,IDC_SSH2_DISAM,sSH2_create,sSH2_destroy,sSH2_save_window,sSH2_restore_window);
    puts("PASS: both SH2 dialogs resize, enforce minimum size, restore bounds and open/closed state, destroy without WM_QUIT");
}
