#include <assert.h>
#include <string.h>
#include "../src/Gens/kmod/planes.c"
unsigned char VRam[65536], CRam[256], VSRam[256];
unsigned short MD_Screen[336 * 240];
int Mode_555;
struct Reg_VDP_Type VDP_Reg;
UCHAR OpenedWindow_KMod[WIN_NUMBER];
HWND HandleWindow_KMod[WIN_NUMBER];
void CloseWindow_KMod(UCHAR mode) { OpenedWindow_KMod[mode-1] = 0; planes_show(2, FALSE); }
int main(void)
{
    PlaneExplorer *e = &explorers[2];
    unsigned short *sat = (unsigned short *)(VRam + 0xF000);
    char temp[MAX_PATH], config[MAX_PATH];
    VDP_Reg.Set4 = 1; VDP_Reg.Spr_Att_Adr = 0x78;
    planes_create(GetModuleHandle(NULL), NULL);
    assert(e->hwnd && HandleWindow_KMod[DMODE_SPRITE_SCREEN-1] == e->hwnd);
    /* Two linked sprites overlap. Transparent pixels reveal the second sprite. */
    sat[0] = 138; sat[1] = 1; sat[2] = 1 | 0x2000; sat[3] = 148;
    sat[4] = 138; sat[5] = 0; sat[6] = 2; sat[7] = 148;
    memset(VRam + 32, 0x11, 32); VRam[33] = 0x01;
    memset(VRam + 64, 0x22, 32);
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[10*1024+20] == 2);
    assert(e->data[10*1024+21] == 17);
    assert(e->data[0] == 0);
    {
        HDC dc = GetDC(e->hwnd), memory = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, 320, 224);
        HBITMAP old = SelectObject(memory, bitmap);
        DRAWITEMSTRUCT draw = {0};
        draw.hDC = memory; SetRect(&draw.rcItem, 0, 0, 320, 224);
        CRam[17 * 2] = 0x0E;
        PlaneExplorerPaint_KMod(e, &draw);
        assert(GetPixel(memory, 21, 10) == RGB(0xEE,0,0));
        assert(GetPixel(memory, 0, 0) == RGB(0,0,0));
        planes_select_sprite(0);
        PlaneExplorerPaint_KMod(e, &draw);
        assert(GetPixel(memory, 20, 10) == RGB(255,0,0));
        assert(GetPixel(memory, 23, 13) == RGB(0xEE,0,0));
        planes_select_sprite(1);
        PlaneExplorerPaint_KMod(e, &draw);
        assert(e->has_selected_sprite);
        planes_select_sprite(79);
        PlaneExplorerPaint_KMod(e, &draw);
        assert(!e->has_selected_sprite);
        assert(GetPixel(memory, 21, 10) == RGB(0xEE,0,0));
        planes_select_sprite(-1);
        SelectObject(memory, old); DeleteObject(bitmap);
        bitmap = CreateCompatibleBitmap(dc, 788, 605);
        old = SelectObject(memory, bitmap);
        SetRect(&draw.rcItem, 0, 0, 788, 605);
        PlaneExplorerPaint_KMod(e, &draw);
        /* Same size/aspect as the reported empty panel. */
        assert(GetPixel(memory, 53, 52) == RGB(0xEE,0,0));
        planes_select_sprite(0);
        PlaneExplorerPaint_KMod(e, &draw);
        assert(GetPixel(memory, 49, 51) == RGB(255,0,0));
        planes_select_sprite(-1);
        SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(e->hwnd, dc);
    }
    /* Horizontal and vertical flips, and column-major multi-tile layout. */
    sat[1] = 0; sat[2] = 1 | 0x2800;
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[10*1024+27] == 0);
    assert(e->data[10*1024+20] == 17);
    sat[2] = 1 | 0x1000;
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[17*1024+20] == 0);
    sat[1] = 0x0500; sat[2] = 1;
    memset(VRam + 32, 0x11, 32); memset(VRam + 96, 0x33, 32);
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[10*1024+20] == 1);
    assert(e->data[18*1024+20] == 2);
    assert(e->data[10*1024+28] == 3);
    /* Cycles terminate; offscreen sprites are clipped; checkerboard is optional. */
    sat[1] = 1; sat[5] = 1; sat[3] = 127;
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[10*1024] == 1);
    e->show_transparence = TRUE;
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[0] >= 253);
    /* Double interlace uses 16-pixel tiles and doubled Y origin. */
    memset(VRam, 0, sizeof(VRam)); VDP_Reg.Set4 = 7;
    sat[0] = 266; sat[2] = 1; sat[3] = 148;
    memset(VRam + 64, 0x44, 64);
    SpriteScreen_UpdateBitmap(e);
    assert(e->data[25*1024+20] == 4);
    assert(GetTempPath(sizeof(temp), temp));
    assert(GetTempFileName(temp, "spr", 0, config));
    OpenedWindow_KMod[DMODE_SPRITE_SCREEN-1] = TRUE;
    planes_show(2, TRUE); planes_save_visibility(config); planes_destroy();
    planes_create(GetModuleHandle(NULL), NULL); planes_restore_visibility(config);
    assert(IsWindowVisible(explorers[2].hwnd));
    SendMessage(explorers[2].hwnd, WM_CLOSE, 0, 0);
    assert(!OpenedWindow_KMod[DMODE_SPRITE_SCREEN-1]);
    planes_destroy(); DeleteFile(config);
    puts("PASS: sprite screen positions, transparency, palette, overlap, flips, tile order, clipping, cycles, interlace and persistence");
    return 0;
}
