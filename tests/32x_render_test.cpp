#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/Gens/gens.h"
#include "../src/Gens/G_gfx.h"
#include "../src/Gens/G_gdi.h"
#include "../src/Gens/G_dsound.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Cpu_SH2.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/cd_file.h"
#include "../src/Gens/cd_hle.h"
#include "../src/Gens/rom.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/vdp_32X.h"
#include "../src/Gens/vdp_rend.h"
#include "../src/Gens/io.h"
#include "../src/Gens/pwm.h"
#include "../src/Gens/psg.h"
#include "../src/Gens/ym2612.h"
#include "../src/Gens/cd_sys.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)

static void word(unsigned a, unsigned v) { *(unsigned short *)&VRam[a] = (unsigned short)v; }

static void setup(int h40) {
    Reset_VDP();
    memset(_32X_VDP_Ram, 0, sizeof(_32X_VDP_Ram));
    memset(MD_Screen, 0x55, sizeof(MD_Screen));
    Set_VDP_Reg(1, 0x40); Set_VDP_Reg(2, 0x30); Set_VDP_Reg(3, 0x2c);
    Set_VDP_Reg(4, 7); Set_VDP_Reg(5, 0x50); Set_VDP_Reg(12, h40);
    Set_VDP_Reg(13, 0x3f); Set_VDP_Reg(16, 1);
    ActiveLayer = ActivePal = 0xff;
    CRam_Flag = 1; VRam_Flag = 3; VDP_Current_Line = 0;
}

static void render_tests() {
    // Every format must consume all 320 pixels even when the MD is in H32.
    for (int h40 = 0; h40 < 2; ++h40) for (int bank = 0; bank < 2; ++bank)
    for (int mode = 1; mode <= 3; ++mode) for (int shift = 0; shift < 2; ++shift) {
        setup(h40);
        Set_VDP_Reg(1, 0); // No MD tile overdraw in the output-boundary test.
        unsigned short *fb = (unsigned short *)(_32X_VDP_Ram + bank * 0x20000);
        fb[0] = 256;
        _32X_VDP.State = bank; _32X_VDP.Mode = mode | (shift << 16);
        for (int i = 0; i < 256; ++i) {
            _32X_VDP_CRam[i] = 0x8000 | i;
            _32X_VDP_CRam_Ajusted[i] = 0x1000 | i;
        }
        for (int i = 0; i <= 320; ++i) {
            if (mode == 2) fb[256 + i] = 0x8000 | i;
            else if (mode == 3) fb[256 + i] = i & 255; // one-pixel RLE runs
            else ((unsigned char *)(fb + 256))[i ^ 1] = (unsigned char)i;
        }
        Render_Line_32X();
        for (int x = 0; x < 320; ++x) {
            unsigned expected = mode == 2 ? _32X_Palette_16B[0x8000 | x]
                : 0x1000 | ((x + (mode == 1 ? shift : 0)) & 255);
            CHECK(MD_Screen[8 + x] == expected);
        }
        CHECK(MD_Screen[7] == 0x5555 && MD_Screen[328] == 0x5555 && MD_Screen[336] == 0x5555);
    }

    // A 256-word direct-color stride exposes the following row at x=256.
    setup(0);
    unsigned short *fb = (unsigned short *)_32X_VDP_Ram;
    fb[0] = 256; fb[1] = 512;
    for (int i = 0; i < 256; ++i) { fb[256+i] = 0x801f; fb[512+i] = 0xfc00; }
    _32X_VDP.State = 0; _32X_VDP.Mode = 2;
    Render_Line_32X();
    CHECK(MD_Screen[8+255] == _32X_Palette_16B[0x801f]);
    CHECK(MD_Screen[8+256] == _32X_Palette_16B[0xfc00]);
    CHECK(MD_Screen[8+319] == _32X_Palette_16B[0xfc00]);

    // MD pixels, including their priority metadata, are expanded before mixing.
    for (int h40 = 0; h40 < 2; ++h40) for (int shadow = 0; shadow < 2; ++shadow) {
        setup(h40);
        Set_VDP_Reg(12, h40 | (shadow << 3));
        for (int i = 1; i < 16; ++i) {
            memset(VRam + i*32, i*17, 32);
            *(unsigned short *)&CRam[i*2] = (unsigned short)(i*0x22);
        }
        for (int x = 0; x < 64; ++x) word(0xc000+x*2, x%16);
        Render_Line();
        unsigned short md[320]; memcpy(md, MD_Screen+8, sizeof(md));
        for (int mode = 0; mode <= 2; ++mode) for (int pri = 0; pri < 2; ++pri) {
            fb = (unsigned short *)_32X_VDP_Ram; fb[0] = 256;
            _32X_VDP.State = 0; _32X_VDP.Mode = mode | (pri << 7);
            for (int x = 0; x < 320; ++x) {
                unsigned high = (x & 1) << 15;
                fb[256+x] = (unsigned short)(high | 31);
                if (mode == 1) ((unsigned char *)(fb+256))[x^1] = (unsigned char)((x&1)+1);
            }
            _32X_VDP_CRam[1] = 31; _32X_VDP_CRam[2] = 0x801f;
            _32X_VDP_CRam_Ajusted[1] = _32X_VDP_CRam_Ajusted[2] = _32X_Palette_16B[31];
            Render_Line_32X();
            for (int x = 0; x < 320; ++x) {
                int md_x = h40 ? x : x*4/5;
                bool transparent = (md_x/8)%16 == 0;
                unsigned expected = mode && (transparent || (x&1) != pri) ? _32X_Palette_16B[31] : md[md_x];
                CHECK(MD_Screen[8+x] == expected);
            }
        }
        ActiveLayer &= ~0x10;
        Render_Line_32X();
        for (int x = 0; x < 320; ++x) CHECK(MD_Screen[8+x] == md[h40 ? x : x*4/5]);
    }
    puts("PASS: 320-pixel direct/indexed/shifted/RLE, both banks, stride, H32/H40 MD composition and priority");
}

static int blit_width, blit_offset;
static void record_blit(unsigned char *, int, int width, int, int offset) {
    blit_width = width; blit_offset = offset;
}
extern RECT GetShotRect();

static void output_tests() {
    HWND window = CreateWindowExA(0, "STATIC", "32X renderer test", WS_POPUP,
                                  0, 0, 320, 224, NULL, NULL, GetModuleHandle(NULL), NULL);
    CHECK(window);
    Init_GDI(window);
    Blit_FS = Blit_W = record_blit; BlitScale_X = BlitScale_Y = 1;
    VDP_Num_Vis_Lines = 224; Stretch = 0;
    for (int h40 = 0; h40 < 2; ++h40) for (int system = 0; system < 3; ++system) {
        Set_VDP_Reg(12, h40);
        _32X_Started = system == 1; CD_32X_Active = system == 2;
        int width = system || h40 ? 320 : 256;
        CHECK(Get_Video_Width() == width);
        for (Full_Screen = 0; Full_Screen < 2; ++Full_Screen) {
            Flip_GDI(window);
            CHECK(blit_width == width && blit_offset == (336-width)*2);
        }
        Full_Screen = 1;
        for (Render_FS = 0; Render_FS < 3; ++Render_FS) {
            RECT rect = GetShotRect();
            CHECK(rect.right-rect.left == width*(Render_FS ? 2 : 1));
        }
    }
    // Cartridge and CD32X paused redraw both use the 320-pixel compositor.
    for (int system = 1; system <= 2; ++system) {
        setup(0);
        _32X_Started = system == 1; CD_32X_Active = system == 2;
        unsigned short *fb = (unsigned short *)_32X_VDP_Ram;
        for (int y = 0; y < 256; ++y) fb[y] = 256;
        for (int x = 0; x < 320; ++x) fb[256+x] = 0x801f;
        _32X_VDP.Mode = 2; _32X_VDP.State = 0;
        Do_VDP_Only();
        CHECK(MD_Screen[8+319] == _32X_Palette_16B[0x801f]);
    }
    _32X_Started = CD_32X_Active = 0; Full_Screen = 0;
    Blit_FS = Blit_W = NULL;
    End_GDI(); DestroyWindow(window);
    puts("PASS: GDI window/fullscreen widths, screenshot cropping, cartridge/CD32X paused redraw, MD-only H32");
}

static void screenshot(const char *path) {
    BITMAPFILEHEADER file = {0}; BITMAPINFOHEADER info = {0};
    file.bfType = 0x4d42; file.bfOffBits = sizeof(file)+sizeof(info);
    file.bfSize = file.bfOffBits+320*224*3;
    info.biSize = sizeof(info); info.biWidth = 320; info.biHeight = -224;
    info.biPlanes = 1; info.biBitCount = 24;
    FILE *f = fopen(path, "wb"); CHECK(f);
    fwrite(&file, sizeof(file), 1, f); fwrite(&info, sizeof(info), 1, f);
    for (int y = 0; y < 224; ++y) for (int x = 0; x < 320; ++x) {
        unsigned p = MD_Screen[y*336+8+x];
        unsigned char rgb[3] = {(unsigned char)((p&31)*255/31),
            (unsigned char)(((p>>5)&63)*255/63), (unsigned char)((p>>11)*255/31)};
        fwrite(rgb, 1, 3, f);
    }
    CHECK(!ferror(f)); fclose(f);
}

static void disc_test(const char *image) {
    GetCurrentDirectory(sizeof(Gens_Path)-2, Gens_Path); strcat(Gens_Path, "\\");
    _putenv("GENS_HLE_SAVE_DIR=.");
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0;
    _32X_Genesis_Bios[0]=_32X_Master_Bios[0]=_32X_Slave_Bios[0]=0;
    Sound_Rate=44100; Sound_Enable=0; Country=0; SegaCD_Accurate=0;
    CDDA_Enable=PWM_Enable=YM2612_Enable=PSG_Enable=1; Seg_Lenght=735;
    for (int i=0; i<262; ++i) {
        Sound_Extrapol[i][0]=Seg_Lenght*i/262;
        Sound_Extrapol[i][1]=Seg_Lenght*(i+1)/262-Sound_Extrapol[i][0];
    }
    for (int i=0; i<Seg_Lenght; ++i) Sound_Interpol[i]=262*i/Seg_Lenght;
    Z80_State=1; MSH2_Speed=SSH2_Speed=100;
    MSH2_Init(); SSH2_Init(); M68K_Init(); S68K_Init(); Z80_Init(); FILE_Init();
    Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
    Controller_1_A=Controller_1_B=Controller_1_C=Controller_1_Start=1;
    Controller_1_X=Controller_1_Y=Controller_1_Z=Controller_1_Mode=1;
    SegaCD_Started=Init_SegaCD(image); CHECK(SegaCD_Started);
    for (int i=0; i<1800; ++i) {
        Controller_1_Start=(i>=600 && i<612)?0:1;
        Controller_1_Left=(i>=850 && i<1050)?0:1;
        memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
        Update_Frame(); CHECK(!Paused);
        if (i==1199) screenshot("disc-1200.bmp");
    }
    CHECK(CD_32X_Active && !(VDP_Reg.Set4&1) && (_32X_VDP.Mode&3)==2);
    unsigned short *fb=(unsigned short *)(_32X_VDP_Ram+(_32X_VDP.State&1)*0x20000);
    printf("ISO: MD R12=%02x, 32X mode=%05x, first line=%u, stride=%u words\n",
           VDP_Reg.Set4, _32X_VDP.Mode, fb[0], (unsigned short)(fb[1]-fb[0]));
    CHECK(fb[1]-fb[0]==256);
    screenshot("disc-1800.bmp");
    // Paused redraw/screenshots must keep the CD32X compositor too.
    Do_VDP_Only(); screenshot("disc-redraw.bmp");
    unsigned tail_pixels = 0;
    for (int y=0; y<224; ++y) for (int x=256; x<320; ++x) {
        unsigned color = fb[(fb[y]+x)&0xffff];
        if (color&0x8000) {
            CHECK(MD_Screen[y*336+8+x] == _32X_Palette_16B[color]);
            ++tail_pixels;
        }
    }
    CHECK(tail_pixels > 100);
    printf("PASS: supplied ISO exposes %u foreground pixels in the rightmost 64 columns\n", tail_pixels);
    Free_Rom(Game);
}

int main(int argc, char **argv) {
    Init_Tab();
    RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100;
    Recalculate_Palettes();
    if (argc>1) disc_test(argv[1]);
    render_tests();
    output_tests();
    return 0;
}
