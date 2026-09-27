#ifndef KMOD_SH2_USAGE_GRAPH_H
#define KMOD_SH2_USAGE_GRAPH_H
#include "../SH2.h"
/* Paint offscreen to avoid flicker during frame refreshes. History is sampled
 * by the emulator, never by WM_PAINT (pause/repaint cannot add frames). */
static void SH2Usage_DrawGraph(const DRAWITEMSTRUCT *item, SH2_CONTEXT *cpu, int active)
{
    UINT32 values[SH2_USAGE_HISTORY];
    unsigned int count = active ? SH2_Usage_GetHistory(cpu, values) : 0, i;
    int width = item->rcItem.right - item->rcItem.left;
    int height = item->rcItem.bottom - item->rcItem.top;
    HDC dc = CreateCompatibleDC(item->hDC);
    HBITMAP bitmap = CreateCompatibleBitmap(item->hDC, width, height);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
    HGDIOBJ oldFont = SelectObject(dc, (HFONT)SendMessage(GetParent(item->hwndItem), WM_GETFONT, 0, 0));
    RECT all = {0,0,width,height}, plot = {30,18,width-8,height-18}, bar;
    HBRUSH blue = CreateSolidBrush(RGB(0,0,255));
    char text[64];
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
    FillRect(dc, &all, GetSysColorBrush(COLOR_3DFACE));
    if (count) wsprintf(text, "%lu.%lu %%", values[count-1]/10, values[count-1]%10);
    else lstrcpy(text, "--");
    TextOut(dc, plot.left, 0, text, lstrlen(text));
    FillRect(dc, &plot, GetSysColorBrush(COLOR_WINDOW));
    TextOut(dc, 0, plot.top-6, "100", 3);
    TextOut(dc, 7, (plot.top+plot.bottom)/2-6, "50", 2);
    TextOut(dc, 14, plot.bottom-6, "0", 1);
    if (count) for (i=0; i<count; ++i) {
        int slot = SH2_USAGE_HISTORY-count+i;
        bar.left = plot.left + slot*(plot.right-plot.left)/SH2_USAGE_HISTORY;
        bar.right = plot.left + (slot+1)*(plot.right-plot.left)/SH2_USAGE_HISTORY;
        bar.bottom = plot.bottom;
        bar.top = plot.bottom - min(values[i],1000)*(plot.bottom-plot.top)/1000;
        if (bar.right > bar.left && bar.top < bar.bottom) FillRect(dc,&bar,blue);
    }
    FrameRect(dc, &plot, GetSysColorBrush(COLOR_WINDOWTEXT));
    TextOut(dc, plot.left, plot.bottom+2, "-255", 4);
    TextOut(dc, (plot.left+plot.right)/2-12, plot.bottom+2, "-128", 4);
    TextOut(dc, plot.right-7, plot.bottom+2, "0", 1);
    BitBlt(item->hDC,item->rcItem.left,item->rcItem.top,width,height,dc,0,0,SRCCOPY);
    DeleteObject(blue);
    SelectObject(dc,oldFont); SelectObject(dc,oldBitmap);
    DeleteObject(bitmap); DeleteDC(dc);
}
#endif
