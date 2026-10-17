/* Build the Release solution first, then run from an x86 VS developer prompt:
 * cl /nologo /TC tests\plane_explorer_test.c /Fo"%TEMP%\plane_explorer_test.obj" /Fe"%TEMP%\plane_explorer_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib
 * "%TEMP%\plane_explorer_test.exe"
 * Uses the real dialog resource with synthetic VRAM; no ROM is required.
 */
#include <assert.h>
#include <string.h>
#include "../src/Gens/kmod/planes.c"

unsigned char VRam[65536];
unsigned char CRam[256];
unsigned char VSRam[256];
struct Reg_VDP_Type VDP_Reg;
unsigned short MD_Screen[336 * 240];
int Mode_555;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];

void CloseWindow_KMod(UCHAR mode)
{
    OpenedWindow_KMod[mode - 1] = FALSE;
    planes_show(mode == DMODE_SHADOW ? 4 : mode == DMODE_HIGHLIGHT ? 5 : mode == DMODE_WINDOW ? 3 : mode == DMODE_SPRITE_SCREEN ? 2 : mode == DMODE_PLANE_A ? 0 : 1, FALSE);
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
    RECT expected[PLANE_COUNT], actual;
    MONITORINFO monitor = { sizeof(monitor) };
    int plane;
    assert(GetMonitorInfo(MonitorFromPoint((POINT){0, 0}, MONITOR_DEFAULTTOPRIMARY), &monitor));
    create_session_windows();
    for (plane = 0; plane < PLANE_COUNT; ++plane)
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
    for (plane = 0; plane < PLANE_COUNT; ++plane)
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
    for (plane = 0; plane < PLANE_COUNT; ++plane)
        explorers[plane].show_transparence = TRUE;
    planes_restore_visibility(config_file);
    for (plane = 0; plane < PLANE_COUNT; ++plane)
    {
        assert(!explorers[plane].show_transparence);
        assert(IsDlgButtonChecked(explorers[plane].hwnd, IDC_PLANEEXPLORER_TRANS) == BST_UNCHECKED);
    }
    assert(!IsWindowVisible(explorers[0].hwnd));
    assert(!IsWindowVisible(explorers[1].hwnd));
    planes_destroy();

    for (mask = 0; mask < (1 << PLANE_COUNT); ++mask)
    {
        create_session_windows();
        for (plane = 0; plane < PLANE_COUNT; ++plane)
        {
            BOOL visible = (mask & (1 << plane)) != 0;
            OpenedWindow_KMod[PlaneExplorerMode(plane) - 1] = visible;
            planes_show(plane, visible);
            /* Exercise checkbox commands, including options on hidden windows. */
            CheckDlgButton(explorers[plane].hwnd, IDC_PLANEEXPLORER_TRANS,
                visible ? BST_UNCHECKED : BST_CHECKED);
            SendMessage(explorers[plane].hwnd, WM_COMMAND,
                MAKEWPARAM(IDC_PLANEEXPLORER_TRANS, BN_CLICKED), 0);
        }
        planes_save_visibility(config_file);
        planes_destroy();
        memset(explorers, 0, sizeof(explorers)); /* Simulate a fresh process. */
        create_session_windows();
        planes_restore_visibility(config_file);
        planes_reset(); /* Loading a ROM must preserve restored visibility. */
        for (plane = 0; plane < PLANE_COUNT; ++plane)
        {
            BOOL expected = (mask & (1 << plane)) != 0;
            assert(IsWindowVisible(explorers[plane].hwnd) == expected);
            assert(OpenedWindow_KMod[PlaneExplorerMode(plane) - 1] == expected);
            assert(explorers[plane].show_transparence == !expected);
            assert(IsDlgButtonChecked(explorers[plane].hwnd, IDC_PLANEEXPLORER_TRANS) ==
                (expected ? BST_UNCHECKED : BST_CHECKED));
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

    /* Viewport borders follow independent scroll values and wrap at map edges. */
    {
        DRAWITEMSTRUCT item = {0};
        item.hDC = dc; item.rcItem.right = 64; item.rcItem.bottom = 64;
        VDP_Reg.Set2 = 0x40;
        VDP_Reg.Scr_Size = 1; /* 512 x 256 */
        VDP_Reg.H_Scr_Adr = 0x3f;
        *(unsigned short *)&VRam[0xfc00] = 16;
        *(unsigned short *)&VSRam[0] = 240;
        PlaneExplorer_DrawViewport(&explorers[0], &item);
        assert(viewport_mask[240 * 1024 + 496]);
        assert(viewport_mask[0]);
        assert(!viewport_mask[220 * 1024]);
        assert(!viewport_mask[300]);
        assert(GetPixel(dc, 0, 0) == RGB(255, 0, 0));
        PlaneExplorer_DrawViewport(&explorers[1], &item);
        assert(viewport_mask[0] && !viewport_mask[496]);
        VDP_Reg.Set3 = 3;
        *(unsigned short *)&VRam[0xfc04] = 32;
        PlaneExplorer_DrawViewport(&explorers[0], &item);
        assert(viewport_mask[241 * 1024 + 480]);
        VDP_Reg.Set3 = 4;
        *(unsigned short *)&VSRam[4] = 32;
        PlaneExplorer_DrawViewport(&explorers[0], &item);
        assert(viewport_mask[32 * 1024]);
        VDP_Reg.Set4 = 7; VDP_Reg.Set2 = 0x48;
        PlaneExplorer_DrawViewport(&explorers[0], &item);
        assert(viewport_mask[480 * 1024 + 496]);
        /* Window must mirror Plane A even with different map dimensions. */
        {
            static unsigned char expected[sizeof(viewport_mask)];
            int mode;
            for (mode = 0; mode < 2; ++mode) {
                VDP_Reg.Set4 = mode ? 7 : 0;
                VDP_Reg.Set2 = mode ? 0x48 : 0x40;
                VDP_Reg.Win_H_Pos = mode ? 0x82 : 0;
                VDP_Reg.Win_V_Pos = mode ? 0x83 : 0;
                PlaneExplorer_DrawViewport(&explorers[0], &item);
                memcpy(expected, viewport_mask, sizeof(expected));
                PlaneExplorer_DrawViewport(&explorers[3], &item);
                assert(memcmp(expected, viewport_mask, sizeof(expected)) == 0);
            }
            VDP_Reg.Win_H_Pos = VDP_Reg.Win_V_Pos = 0;
        }
        VDP_Reg.Set4 = VDP_Reg.Set2 = VDP_Reg.Set3 = VDP_Reg.Scr_Size = 0;
        memset(VSRam, 0, sizeof(VSRam));
        /* Raster split: preserve top and bottom scrolls even after VSRAM changes. */
        VDP_Reg.Set2 = 0x40; VDP_Reg.Scr_Size = 0x11;
        *(unsigned short *)&VRam[0xfc00] = 0;
        *(unsigned short *)&VRam[0xfc02] = 0;
        *(unsigned short *)&VSRam[0] = 0;
        *(unsigned short *)&VSRam[2] = 16;
        planes_capture_intensity(0);
        *(unsigned short *)&VSRam[0] = 128;
        *(unsigned short *)&VSRam[2] = 256;
        planes_capture_intensity(112 * 336);
        memset(VSRam, 0, sizeof(VSRam));
        PlaneExplorer_DrawViewport(&explorers[0], &item);
        assert(viewport_mask[0] && viewport_mask[240 * 1024]);
        assert(!viewport_mask[112 * 1024]);
        PlaneExplorer_DrawViewport(&explorers[3], &item);
        assert(viewport_mask[0] && viewport_mask[240 * 1024]);
        assert(!viewport_mask[112 * 1024]);
        PlaneExplorer_DrawViewport(&explorers[1], &item);
        assert(viewport_mask[16 * 1024] && viewport_mask[368 * 1024]);
        assert(!viewport_mask[0]);
        memset(scroll_lines, 0, sizeof(scroll_lines));
        VDP_Reg.Set2 = VDP_Reg.Scr_Size = 0;

    }
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

    /* Window has its own map, fixed height and H32/H40 address alignment. */
    {
        HWND w = HandleWindow_KMod[DMODE_WINDOW - 1];
        assert(w && w != a && w != b);
        GetWindowText(w, text, sizeof(text)); assert(strcmp(text, "Window") == 0);
        VDP_Reg.Pat_Win_Adr = 0x2e;
        VDP_Reg.Scr_Size = 0x11;
        *(unsigned short *)&VRam[0xb800] = 1;
        paint(w, dc); assert(GetPixel(dc, 0, 0) == RGB(238, 0, 0));
        assert(explorers[3].old_width == 32 && explorers[3].old_height == 32);
        PlaneExplorer_GetTipText(&explorers[3], 0, 0, text);
        assert(strstr(text, "Window") && strstr(text, "0xB800"));
        VDP_Reg.Set4 = 1;
        *(unsigned short *)&VRam[0xb000] = 2;
        paint(w, dc); assert(GetPixel(dc, 0, 0) == RGB(0, 238, 0));
        assert(explorers[3].old_width == 64 && explorers[3].old_height == 32);
        planes_show(3, TRUE); SendMessage(w, WM_CLOSE, 0, 0);
        assert(!IsWindowVisible(w));
        VDP_Reg.Set4 = 0; VDP_Reg.Scr_Size = 0;
    }

    /* Classify composed palette indices, including normal 0xc0, then
     * verify the two independent masks use the H32 viewport starts at pixel 8, just like H40. */
    {
        DRAWITEMSTRUCT item = {0};
        void *effect_pixels;
        BITMAPINFO info = {0};
        HDC effect_dc = CreateCompatibleDC(NULL);
        HBITMAP effect_bitmap, old_bitmap;
        info.bmiHeader.biSize = 40; info.bmiHeader.biWidth = 256;
        info.bmiHeader.biHeight = -224; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        effect_bitmap = CreateDIBSection(effect_dc, &info, DIB_RGB_COLORS, &effect_pixels, NULL, 0);
        old_bitmap = SelectObject(effect_dc, effect_bitmap);
        VDP_Reg.Set2 = 0x40; VDP_Reg.Set4 = 8;
        MD_Screen[8] = 0x40; MD_Screen[9] = 0x80; MD_Screen[10] = 0xc0;
        MD_Screen[263] = 0x80; MD_Screen[264] = 0x40;
        planes_capture_intensity(0);
        assert(intensity_map[0] == 0x40 && intensity_map[1] == 0x80 && intensity_map[2] == 0xc0 && intensity_map[255] == 0x80);
        item.hDC = effect_dc; item.rcItem.right = 256; item.rcItem.bottom = 224;
        IntensityPaint(&explorers[4], &item);
        assert(GetPixel(effect_dc, 0, 0) == RGB(0,255,255));
        assert(GetPixel(effect_dc, 1, 0) != RGB(0,255,255));
        GetDlgItemText(explorers[4].hwnd, IDC_PLANEEXPLORER_PROPS, text, sizeof(text));
        assert(strstr(text, "1 pixels") && strstr(text, "S/H enabled"));
        IntensityPaint(&explorers[5], &item);
        assert(GetPixel(effect_dc, 1, 0) == RGB(255,192,64));
        assert(GetPixel(effect_dc, 2, 0) != RGB(255,192,64));
        VDP_Reg.Set4 = 0; planes_capture_intensity(0);
        assert(intensity_map[0] == 0 && !intensity_enabled);
        VDP_Reg.Set4 = 9; MD_Screen[8] = 0x40; planes_capture_intensity(0);
        assert(intensity_width == 320 && intensity_map[0] == 0x40);
        VDP_Reg.Set2 = 0; planes_capture_intensity(0); assert(intensity_map[0] == 0);
        VDP_Reg.Set4 = 0;
        SelectObject(effect_dc, old_bitmap); DeleteObject(effect_bitmap); DeleteDC(effect_dc);
    }

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
