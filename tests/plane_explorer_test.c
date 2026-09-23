/* Build the Release solution first, then run from an x86 VS developer prompt:
 * cl /nologo /TC tests\plane_explorer_test.c /Fo"%TEMP%\plane_explorer_test.obj" /Fe"%TEMP%\plane_explorer_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib
 * "%TEMP%\plane_explorer_test.exe"
 * Uses the real dialog resource with synthetic VRAM; no ROM is required.
 */
#include <assert.h>
#include <string.h>
#include "../src/Gens/kmod/planes.c"

unsigned char VRam[64000];
unsigned char CRam[256];
struct Reg_VDP_Type VDP_Reg;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];

void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    planes_show(mode == DMODE_PLANE_A ? 0 : 1, FALSE);
}

static void paint(HWND hwnd, HDC dc)
{
    DRAWITEMSTRUCT item = {0};
    item.CtlID = IDC_PLANEXPLEORER_MAIN;
    item.hDC = dc;
    item.rcItem.right = 64;
    item.rcItem.bottom = 64;
    SendMessage(hwnd, WM_DRAWITEM, item.CtlID, (LPARAM)&item);
}

static void create_session_windows(void)
{
    int plane;
    planes_create(GetModuleHandle(NULL), NULL);
    for (plane = 0; plane < PLANE_COUNT; ++plane)
    {
        assert(explorers[plane].hwnd);
        SetWindowPos(explorers[plane].hwnd, NULL, -10000, -10000, 500, 400,
            SWP_NOACTIVATE | SWP_NOZORDER);
    }
}

static void test_geometry_persistence(const char *config_file)
{
    RECT expected[2], actual;
    MONITORINFO monitor = { sizeof(monitor) };
    int plane;
    assert(GetMonitorInfo(MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor));
    create_session_windows();
    for (plane = 0; plane < 2; ++plane)
    {
        SetWindowPos(explorers[plane].hwnd, NULL,
            monitor.rcWork.left + 30 + plane * 40, monitor.rcWork.top + 30 + plane * 40,
            450 + plane * 100, 300 + plane * 50, SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(explorers[plane].hwnd, &expected[plane]);
    }
    planes_save_visibility(config_file);
    planes_destroy();
    create_session_windows();
    planes_restore_visibility(config_file);
    for (plane = 0; plane < 2; ++plane)
    {
        GetWindowRect(explorers[plane].hwnd, &actual);
        assert(EqualRect(&actual, &expected[plane]));
        assert(!IsWindowVisible(explorers[plane].hwnd));
    }
    /* Maximizing must save the normal rectangle, not the entire desktop. */
    ShowWindow(explorers[0].hwnd, SW_SHOWMAXIMIZED);
    planes_save_visibility(config_file);
    planes_destroy();
    create_session_windows();
    planes_restore_visibility(config_file);
    GetWindowRect(explorers[0].hwnd, &actual);
    assert(EqualRect(&actual, &expected[0]));
    assert(!IsZoomed(explorers[0].hwnd));
    /* A disconnected monitor or oversized window must still be reachable. */
    WritePrivateProfileString("DebugWindows", "PlaneARect", "900000,900000,50000,50000", config_file);
    planes_restore_visibility(config_file);
    GetWindowRect(explorers[0].hwnd, &actual);
    GetMonitorInfo(MonitorFromWindow(explorers[0].hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    assert(actual.left >= monitor.rcWork.left && actual.top >= monitor.rcWork.top);
    assert(actual.right <= monitor.rcWork.right && actual.bottom <= monitor.rcWork.bottom);
    WritePrivateProfileString("DebugWindows", "PlaneARect", "invalid", config_file);
    planes_restore_visibility(config_file);
    GetWindowRect(explorers[0].hwnd, &expected[0]);
    assert(EqualRect(&actual, &expected[0]));
    planes_destroy();
}

static void test_visibility_persistence(void)
{
    char temp_dir[MAX_PATH], config_file[MAX_PATH], value[32];
    int mask, plane;
    assert(GetTempPath(sizeof(temp_dir), temp_dir));
    assert(GetTempFileName(temp_dir, "gkp", 0, config_file));
    WritePrivateProfileString("Debug", "Spy", "123", config_file);

    create_session_windows();
    planes_restore_visibility(config_file);
    assert(!IsWindowVisible(explorers[0].hwnd));
    assert(!IsWindowVisible(explorers[1].hwnd));
    planes_destroy();

    for (mask = 0; mask < 4; ++mask)
    {
        create_session_windows();
        for (plane = 0; plane < PLANE_COUNT; ++plane)
        {
            BOOL visible = (mask & (1 << plane)) != 0;
            OpenedWindow_KMod[PlaneExplorerMode(plane) - 1] = visible;
            planes_show(plane, visible);
        }
        planes_save_visibility(config_file);
        planes_destroy();
        create_session_windows();
        planes_restore_visibility(config_file);
        planes_reset(); /* Loading a ROM must preserve restored visibility. */
        for (plane = 0; plane < PLANE_COUNT; ++plane)
        {
            BOOL expected = (mask & (1 << plane)) != 0;
            assert(IsWindowVisible(explorers[plane].hwnd) == expected);
            assert(OpenedWindow_KMod[PlaneExplorerMode(plane) - 1] == expected);
        }
        planes_destroy();
    }

    /* Explicitly closing A must replace the previously saved open state. */
    create_session_windows();
    planes_restore_visibility(config_file);
    SendMessage(explorers[0].hwnd, WM_CLOSE, 0, 0);
    planes_save_visibility(config_file);
    planes_destroy();
    create_session_windows();
    planes_restore_visibility(config_file);
    assert(!IsWindowVisible(explorers[0].hwnd));
    assert(IsWindowVisible(explorers[1].hwnd));
    planes_destroy();
    GetPrivateProfileString("Debug", "Spy", "", value, sizeof(value), config_file);
    assert(strcmp(value, "123") == 0);
    test_geometry_persistence(config_file);
    assert(DeleteFile(config_file));
}

int main(void)
{
    HWND a, b;
    char text[1024];
    MSG msg;
    HDC dc = CreateCompatibleDC(NULL);
    BITMAPINFO bmi = {0};
    void *pixels;
    HBITMAP bitmap, previous;

    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 64;
    bmi.bmiHeader.biHeight = -64;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bitmap = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    assert(dc && bitmap);
    previous = (HBITMAP)SelectObject(dc, bitmap);

    VDP_Reg.Pat_ScrA_Adr = 0x30;
    VDP_Reg.Pat_ScrB_Adr = 7;
    *(unsigned short *)&VRam[0xC000] = 1;
    *(unsigned short *)&VRam[0xE000] = 2;
    memset(VRam + 32, 0x11, 32);
    memset(VRam + 64, 0x22, 32);
    CRam[2] = 0x0E;
    CRam[4] = 0xE0;

    planes_create(GetModuleHandle(NULL), NULL);
    a = HandleWindow_KMod[DMODE_PLANE_A - 1];
    b = HandleWindow_KMod[DMODE_PLANE_B - 1];
    assert(a && b && a != b);
    GetWindowText(a, text, sizeof(text)); assert(strcmp(text, "Plane A") == 0);
    GetWindowText(b, text, sizeof(text)); assert(strcmp(text, "Plane B") == 0);
    assert(!GetDlgItem(a, 54002) && !GetDlgItem(b, 54002));

    /* Keep the test windows off-screen while checking visibility/lifecycle. */
    SetWindowPos(a, NULL, -10000, -10000, 500, 400, SWP_NOACTIVATE | SWP_NOZORDER);
    SetWindowPos(b, NULL, -10000, -10000, 500, 400, SWP_NOACTIVATE | SWP_NOZORDER);
    OpenedWindow_KMod[DMODE_PLANE_A - 1] = TRUE;
    OpenedWindow_KMod[DMODE_PLANE_B - 1] = TRUE;
    planes_show(0, TRUE); planes_show(1, TRUE);
    assert(IsWindowVisible(a) && IsWindowVisible(b));

    paint(a, dc); assert(GetPixel(dc, 0, 0) == RGB(238, 0, 0));
    paint(b, dc); assert(GetPixel(dc, 0, 0) == RGB(0, 238, 0));
    paint(a, dc); assert(GetPixel(dc, 0, 0) == RGB(238, 0, 0));
    GetDlgItemText(a, IDC_PLANEEXPLORER_PROPS, text, sizeof(text));
    assert(strstr(text, "0xC000"));
    GetDlgItemText(b, IDC_PLANEEXPLORER_PROPS, text, sizeof(text));
    assert(strstr(text, "0xE000"));
    PlaneExplorer_GetTipText(&explorers[0], 0, 0, text);
    assert(strstr(text, "plane A") && strstr(text, "tile 1,"));
    PlaneExplorer_GetTipText(&explorers[1], 0, 0, text);
    assert(strstr(text, "plane B") && strstr(text, "tile 2,"));

    CheckDlgButton(a, IDC_PLANEEXPLORER_TRANS, BST_CHECKED);
    SendMessage(a, WM_COMMAND, MAKEWPARAM(IDC_PLANEEXPLORER_TRANS, BN_CLICKED),
        (LPARAM)GetDlgItem(a, IDC_PLANEEXPLORER_TRANS));
    paint(a, dc); assert(GetPixel(dc, 8, 0) == RGB(68, 68, 68));
    paint(b, dc); assert(GetPixel(dc, 8, 0) == RGB(0, 0, 0));
    assert(IsDlgButtonChecked(b, IDC_PLANEEXPLORER_TRANS) == BST_UNCHECKED);

    SendMessage(a, WM_CLOSE, 0, 0);
    assert(!IsWindowVisible(a) && IsWindowVisible(b));
    assert(!OpenedWindow_KMod[DMODE_PLANE_A - 1]);
    assert(OpenedWindow_KMod[DMODE_PLANE_B - 1]);
    OpenedWindow_KMod[DMODE_PLANE_A - 1] = TRUE;
    planes_show(0, TRUE);
    assert(IsWindowVisible(a) && IsWindowVisible(b));
    planes_reset();
    paint(a, dc); paint(b, dc);
    assert(explorers[0].data[0] == 1 && explorers[1].data[0] == 2);
    assert(IsDlgButtonChecked(a, IDC_PLANEEXPLORER_TRANS) == BST_CHECKED);

    VDP_Reg.Set4 = 6;
    paint(a, dc); paint(b, dc);
    GetDlgItemText(b, IDC_PLANEEXPLORER_PROPS, text, sizeof(text));
    assert(strstr(text, "Double interlaced"));
    PlaneExplorer_GetTipText(&explorers[1], 0, 8, text);
    assert(strstr(text, "0, 0 in plane B"));
    /* Maximum interlaced height must fit each independent bitmap. */
    VDP_Reg.Pat_ScrA_Adr = 0;
    VDP_Reg.Pat_ScrB_Adr = 1;
    VDP_Reg.Scr_Size = 0x33;
    paint(a, dc); paint(b, dc);
    assert(explorers[0].old_height == 128 && explorers[1].old_tile_height == 16);

    SendMessage(b, WM_COMMAND, IDCANCEL, 0);
    assert(IsWindowVisible(a) && !IsWindowVisible(b));
    DestroyWindow(a);
    assert(IsWindow(b) && !HandleWindow_KMod[DMODE_PLANE_A - 1]);
    planes_destroy();
    assert(!HandleWindow_KMod[DMODE_PLANE_B - 1]);
    assert(!PeekMessage(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
    SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
    test_visibility_persistence();
    puts("PASS: independent rendering, transparency, tile info, reset, interlace, close/reopen, destruction, visibility persistence");
    return 0;
}
