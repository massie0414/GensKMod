#ifndef KMOD_SH2_WINDOW_LAYOUT_H
#define KMOD_SH2_WINDOW_LAYOUT_H
#include <windows.h>
/* Both SH2 dialogs share the same resource layout. Keep original pixel
 * bounds so repeated resize operations never accumulate rounding errors. */
typedef struct {
    SIZE minimum, client;
    int count, listId, scrollId, rightEdge, bottomEdge;
    struct { HWND hwnd; RECT rect; } controls[20];
} SH2WindowLayout;
/* OEM_FIXED_FONT can be taller/wider than the resource dialog font.
 * Measure it after WM_SETFONT, before capturing the resize anchors. */
static RECT SH2Window_ControlRect(HWND hwnd, int id)
{
    RECT r;
    GetWindowRect(GetDlgItem(hwnd, id), &r);
    MapWindowPoints(NULL, hwnd, (POINT *)&r, 2);
    return r;
}
static void SH2Window_FitStatus(HWND hwnd, int srId, int adrId, int dataId, int usageId)
{
    const char *samples[3] = {
        "T=0 S=0 Q=0 M=0 I=0 SR=0000 Status=0000",
        "R0=FFFFFFFF R1=FFFFFFFF R2=FFFFFFFF R3=FFFFFFFF",
        "GBR=FFFFFFFF VBR=FFFFFFFF PR=FFFFFFFF"
    };
    int ids[3], lines[3] = {1, 4, 3};
    int i, top, right = 0, dx, dy, width, height;
    RECT r, client, outer, group;
    HDC dc = GetDC(hwnd);
    ids[0] = srId; ids[1] = adrId; ids[2] = dataId;
    r = SH2Window_ControlRect(hwnd, srId);
    top = r.top;
    for (i = 0; i < 3; ++i) {
        TEXTMETRIC metrics;
        SIZE extent;
        HFONT previous = (HFONT)SelectObject(dc,
            (HFONT)SendDlgItemMessage(hwnd, ids[i], WM_GETFONT, 0, 0));
        GetTextMetrics(dc, &metrics);
        GetTextExtentPoint32(dc, samples[i], lstrlen(samples[i]), &extent);
        SelectObject(dc, previous);
        r = SH2Window_ControlRect(hwnd, ids[i]);
        width = extent.cx + 4;
        height = metrics.tmHeight * lines[i] + 2;
        MoveWindow(GetDlgItem(hwnd, ids[i]), r.left, top, width, height, FALSE);
        right = max(right, r.left + width);
        top += height + 4;
    }
    ReleaseDC(hwnd, dc);
    group = SH2Window_ControlRect(hwnd, IDC_SH2_USAGE_GROUP);
    r = SH2Window_ControlRect(hwnd, adrId);
    dx = max(0, right + 8 - group.left);
    dy = r.top - group.top;
    MoveWindow(GetDlgItem(hwnd, IDC_SH2_USAGE_GROUP), group.left + dx,
        group.top + dy, group.right - group.left, max(group.bottom - group.top, top - 4 - r.top), FALSE);
    ids[0] = usageId; ids[1] = IDC_SH2_USAGE_NOTE;
    for (i = 0; i < 2; ++i) {
        r = SH2Window_ControlRect(hwnd, ids[i]);
        MoveWindow(GetDlgItem(hwnd, ids[i]), r.left + dx, r.top + dy,
            r.right - r.left, r.bottom - r.top, FALSE);
    }
    GetClientRect(hwnd, &client);
    GetWindowRect(hwnd, &outer);
    width = max(client.right, group.right + dx + 8);
    height = max(client.bottom, max(top + 4, group.bottom + dy + 8));
    SetWindowPos(hwnd, NULL, 0, 0, outer.right - outer.left + width - client.right,
        outer.bottom - outer.top + height - client.bottom,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}
static void SH2Window_InitLayout(HWND hwnd, SH2WindowLayout *layout, int listId, int scrollId)
{
    RECT rect, anchors = {0, 0, 245, 121};
    HWND child;
    ZeroMemory(layout, sizeof(*layout));
    layout->listId = listId; layout->scrollId = scrollId;
    GetWindowRect(hwnd, &rect);
    layout->minimum.cx = rect.right - rect.left;
    layout->minimum.cy = rect.bottom - rect.top;
    GetClientRect(hwnd, &rect);
    layout->client.cx = rect.right; layout->client.cy = rect.bottom;
    MapDialogRect(hwnd, &anchors);
    layout->rightEdge = anchors.right; layout->bottomEdge = anchors.bottom;
    for (child = GetWindow(hwnd, GW_CHILD); child && layout->count < 20;
         child = GetWindow(child, GW_HWNDNEXT)) {
        int i = layout->count++;
        layout->controls[i].hwnd = child;
        GetWindowRect(child, &layout->controls[i].rect);
        MapWindowPoints(NULL, hwnd, (POINT *)&layout->controls[i].rect, 2);
    }
}
static void SH2Window_Resize(HWND hwnd, const SH2WindowLayout *layout)
{
    RECT client;
    int i, dx, dy;
    if (!layout->count) return;
    GetClientRect(hwnd, &client);
    dx = max(0, client.right - layout->client.cx);
    dy = max(0, client.bottom - layout->client.cy);
    for (i = 0; i < layout->count; ++i) {
        RECT r = layout->controls[i].rect;
        int id = GetDlgCtrlID(layout->controls[i].hwnd);
        if (id == layout->listId) { r.right += dx; r.bottom += dy; }
        else if (id == layout->scrollId) { OffsetRect(&r, dx, 0); r.bottom += dy; }
        else OffsetRect(&r, r.left >= layout->rightEdge ? dx : 0,
                        r.top >= layout->bottomEdge ? dy : 0);
        MoveWindow(layout->controls[i].hwnd, r.left, r.top,
                   r.right - r.left, r.bottom - r.top, TRUE);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}
static unsigned int SH2Window_Rows(HWND hwnd, int listId)
{
    RECT rect;
    int height = (int)SendDlgItemMessage(hwnd, listId, LB_GETITEMHEIGHT, 0, 0);
    GetClientRect(GetDlgItem(hwnd, listId), &rect);
    return height > 0 ? max(1, (rect.bottom + height - 1) / height) : 13;
}
#endif
