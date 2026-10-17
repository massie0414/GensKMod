#include <windows.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include "../src/Gens/kmod/common.h"
}
#include "../src/Gens/gens.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/vdp_rend.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/planes.h"

#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void word(unsigned a,unsigned v) { *(unsigned short *)&VRam[a]=v; }
static void dump(const char *name, void *pixels) {
 BITMAPFILEHEADER h={0};BITMAPINFOHEADER b={0};h.bfType=0x4d42;h.bfOffBits=54;h.bfSize=54+320*224*4;
 b.biSize=40;b.biWidth=320;b.biHeight=-224;b.biPlanes=1;b.biBitCount=32;
 FILE *f=fopen(name,"wb");fwrite(&h,14,1,f);fwrite(&b,40,1,f);fwrite(pixels,320*224*4,1,f);fclose(f);
}
int main(void) {
 Init_Tab(); Reset_VDP();
 RMax_Level=GMax_Level=BMax_Level=224;Contrast_Level=Brightness_Level=100;Recalculate_Palettes();
 Set_VDP_Reg(1,0x40); Set_VDP_Reg(2,0x30);Set_VDP_Reg(3,0x2c);Set_VDP_Reg(4,7);
 Set_VDP_Reg(5,0x50);Set_VDP_Reg(12,9);Set_VDP_Reg(13,0x3f);Set_VDP_Reg(16,1);
 memset(VRam,0,sizeof(VRam));memset(VSRam,0,sizeof(VSRam));
 memset(VRam+32,0x11,32);memset(VRam+64,0xee,32);memset(VRam+96,0xff,32);
 for(int i=0;i<64*32;i++){word(0xc000+i*2,1);word(0xe000+i*2,1);}
 // High-priority background starts at x=64; low-priority background remains shadow.
 for(int y=0;y<28;y++)for(int x=8;x<16;x++)word(0xc000+(y*64+x)*2,0x8001);
 word(0xa000,128);word(0xa002,1);word(0xa004,0xe002);word(0xa006,128+64);
 word(0xa008,128);word(0xa00a,0);word(0xa00c,0xe003);word(0xa00e,128+80);
 *(unsigned short *)&CRam[2]=0x0eee;CRam_Flag=1;VRam_Flag=3;ActiveLayer=ActivePal=0xff;
 for(VDP_Current_Line=0;VDP_Current_Line<224;VDP_Current_Line++)Render_Line();
 planes_create(GetModuleHandle(NULL),NULL);
 BITMAPINFO info={0};info.bmiHeader.biSize=40;info.bmiHeader.biWidth=320;info.bmiHeader.biHeight=-224;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 HDC dc=CreateCompatibleDC(NULL);void *pixels;HBITMAP bm=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,NULL,0);HGDIOBJ old=SelectObject(dc,bm);
 DRAWITEMSTRUCT d={0};d.hDC=dc;d.rcItem.right=320;d.rcItem.bottom=224;
 HWND sh=HandleWindow_KMod[DMODE_SHADOW-1],hi=HandleWindow_KMod[DMODE_HIGHLIGHT-1];CHECK(sh && hi);
 SendMessage(sh,WM_DRAWITEM,IDC_PLANEXPLEORER_MAIN,(LPARAM)&d);
 CHECK(GetPixel(dc,0,0)==RGB(0,255,255));CHECK(GetPixel(dc,80,0)==RGB(0,255,255));CHECK(GetPixel(dc,72,0)!=RGB(0,255,255));dump("shadow.bmp",pixels);
 SendMessage(hi,WM_DRAWITEM,IDC_PLANEXPLEORER_MAIN,(LPARAM)&d);
 CHECK(GetPixel(dc,64,0)==RGB(255,192,64));CHECK(GetPixel(dc,72,0)!=RGB(255,192,64));CHECK(GetPixel(dc,80,0)!=RGB(255,192,64));dump("highlight.bmp",pixels);
 // H32 crops the same origin, not 32 pixels farther into the buffer.
 Set_VDP_Reg(12,8);for(VDP_Current_Line=0;VDP_Current_Line<224;VDP_Current_Line++)Render_Line();
 d.rcItem.right=256;
 SendMessage(sh,WM_DRAWITEM,IDC_PLANEXPLEORER_MAIN,(LPARAM)&d);
 CHECK(GetPixel(dc,0,0)==RGB(0,255,255));CHECK(GetPixel(dc,80,0)==RGB(0,255,255));CHECK(GetPixel(dc,72,0)!=RGB(0,255,255));
 SendMessage(hi,WM_DRAWITEM,IDC_PLANEXPLEORER_MAIN,(LPARAM)&d);
 CHECK(GetPixel(dc,64,0)==RGB(255,192,64));CHECK(GetPixel(dc,32,0)!=RGB(255,192,64));
 // Rendering with S/H disabled must clear old classification.
 Set_VDP_Reg(12,1);for(VDP_Current_Line=0;VDP_Current_Line<224;VDP_Current_Line++)Render_Line();
 SendMessage(hi,WM_DRAWITEM,IDC_PLANEXPLEORER_MAIN,(LPARAM)&d);CHECK(GetPixel(dc,64,0)!=RGB(255,192,64));
 planes_destroy();SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);
 puts("PASS: renderer shadow background, highlight/shadow sprite operators, normal priority and mode disable");return 0;
}
