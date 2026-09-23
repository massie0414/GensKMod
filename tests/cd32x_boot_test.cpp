// Integration test for the user-supplied MODPlayer32X disc. No game/BIOS data embedded.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
extern "C" {
#include "../src/Gens/Kmod.h"
#include "../src/Gens/kmod/common.h"
}

#include "../src/Gens/gens.h"
#include "../src/Gens/G_gfx.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/vdp_32x.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Cpu_SH2.h"
#include "../src/Gens/pwm.h"
#include "../src/Gens/32x_boot.h"
#include "../src/Gens/psg.h"
#include "../src/Gens/ym2612.h"
#include "../src/Gens/Mem_SH2.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/Mem_S68K.h"
#include "../src/Gens/cd_file.h"
#include "../src/Gens/cd_hle.h"
#include "../src/Gens/cd_sys.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/G_dsound.h"
#include "../src/Gens/rom.h"
#include "../src/Gens/save.h"
#include "../src/Gens/vdp_rend.h"
#include "../src/Gens/io.h"

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static HWND test_vdp_window;
static BOOL CALLBACK find_test_vdp(HWND window, LPARAM unused) {
    if(GetDlgItem(window,IDC_32XVDP_TILES)) test_vdp_window=window;
    return TRUE;
}
static int live_vdp_test(void) {
    CHECK(CD_32X_Active && !_32X_Started);
    vdp32x_create(GetModuleHandle(NULL),NULL);
    EnumThreadWindows(GetCurrentThreadId(),find_test_vdp,0);
    CHECK(test_vdp_window);
    ShowWindow(test_vdp_window,SW_SHOWNOACTIVATE);
    const int controls[]={IDC_32XVDP_TILES,IDC_32XVDP_TILES2,IDC_32XVDP_PAL};
    OpenedWindow_KMod[DMODE_32_VDP-1]=TRUE;
    for(int frame=0;frame<3;frame++) {
        for(int n=0;n<3;n++) ValidateRect(GetDlgItem(test_vdp_window,controls[n]),NULL);
        Update_KMod();
        for(int n=0;n<3;n++) CHECK(GetUpdateRect(GetDlgItem(test_vdp_window,controls[n]),NULL,FALSE));
    }
    OpenedWindow_KMod[DMODE_32_VDP-1]=FALSE;
    vdp32x_destroy();
    puts("PASS: CD32X periodic debugger update invalidates both framebuffers and palette");
    return 0;
}
static int pulse(int frame,int when) { return frame>=when && frame<when+24; }
static unsigned hash(const unsigned char *p,unsigned n) { unsigned h=2166136261U; while(n--)h=(h^*p++)*16777619U; return h; }
static void release_pad(void) {
    Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
    Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_C=1;
    Controller_1_X=Controller_1_Y=Controller_1_Z=Controller_1_Mode=1;
}
static void main_long(unsigned a,unsigned v) { M68K_WW(a,(unsigned short)(v>>16)); M68K_WW(a+2,(unsigned short)v); }
static int boot_contract_tests(void) {
    const unsigned dsts[]={0x06000100,0x05fffffc,0x06040000,0x06000100,0x06000100};
    const unsigned lens[]={4,4,4,0x20000,3};
    for(unsigned test=0;test<5;test++) {
        CD_32X_Active=0; SegaCD_Started=1; Init_Memory_M68K(2); Enable_CD_32X();
        M68K_WW(0xa15100,3);
        main_long(0x840018,dsts[test]); main_long(0x84001c,lens[test]);
        main_long(0x840020,0x06000100); main_long(0x840024,0x06000100);
        main_long(0x840028,0x06000000); main_long(0x84002c,0x06000000);
        main_long(0x840038,0xaffe0009);
        main_long(0xa15120,0x5f43445f);
        SH2_Exec(&M_SH2,10000); SH2_Exec(&S_SH2,10000);
        unsigned pc=SH2_Get_PC(&M_SH2);
        if(test==0) {
            CHECK(pc>=0x06000100 && pc<=0x06000104);
            CHECK(SH2_Read_Long(&M_SH2,0x06000100)==0xaffe0009);
            CHECK(SH2_Get_VBR(&M_SH2)==0x06000000);
            CHECK(M68K_RW(0xa15120)==0x4d5f && M68K_RW(0xa15124)==0x535f);
        } else CHECK(pc==0x1f0 || pc==0x1f2 || pc==0x1f4);
        CHECK(SH2_Read_Byte(&M_SH2,0x20004000)&1); // nCART
    }
    CD_32X_Active=0; SegaCD_Started=0;
    puts("PASS: framebuffer CD boot handshake, SDRAM bounds, malformed sizes, nCART");
    return 0;
}
static int dma_tests(void) {
    for(int slave=0;slave<2;slave++) {
        SH2_CONTEXT *cpu=slave?&S_SH2:&M_SH2;
        SH2_Reset(cpu,0); SH2_DMA1_Request(cpu,0); PWM_Init(); PWM_Enable=1;
        _32X_MINT=_32X_SINT=0;
        for(unsigned i=0;i<4;i++)SH2_Write_Long(cpu,0x06010000+i*4,0x03000400+i*0x10001);
        SH2_Write_Long(cpu,0xffffffb0,1); // DMAOR
        SH2_Write_Long(cpu,0xffffff90,0x06010000);
        SH2_Write_Long(cpu,0xffffff94,0x20004034); // stereo PWM pair
        SH2_Write_Long(cpu,0xffffff98,4);
        SH2_Write_Long(cpu,0xffffff9c,0x18e1);
        PWM_Mode=0x185; PWM_Set_Cycle(32); PWM_Set_Int(1);
        for(unsigned n=0;n<4;n++) {
            PWM_Update_Timer(n*31);
            CHECK(cpu->TCR1==3-n);
            CHECK(cpu->SAR1==0x06010000+(n+1)*4);
            CHECK((cpu->CHCR1&2)==(n==3?2:0));
        }
        CHECK(PWM_FIFO_L[3]==0x303 && PWM_FIFO_R[3]==0x403);
        // A level request to ordinary RAM must also stop when TCR reaches zero.
        SH2_Write_Long(cpu,0xffffff90,0x06010000);
        SH2_Write_Long(cpu,0xffffff94,0x06010100);
        SH2_Write_Long(cpu,0xffffff98,2);
        SH2_Write_Long(cpu,0xffffff9c,0x58e1); // increment both addresses
        SH2_DMA1_Request(cpu,1);
        CHECK(cpu->TCR1==0 && (cpu->CHCR1&2));
        CHECK(SH2_Read_Long(cpu,0x06010104)==0x03010401);
    }
    puts("PASS: PWM DMA pacing on both SH2s, masked interrupts, transfer completion");
    return 0;
}
static int screenshot(const char *path)
{
    BITMAPFILEHEADER file={0}; BITMAPINFOHEADER info={0};
    file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(info);
    file.bfSize=file.bfOffBits+336*240*3;
    info.biSize=sizeof(info); info.biWidth=336; info.biHeight=240;
    info.biPlanes=1; info.biBitCount=24;
    FILE *f=fopen(path,"wb"); if(!f)return 0;
    fwrite(&file,1,sizeof(file),f); fwrite(&info,1,sizeof(info),f);
    unsigned lit=0;
    for(int y=239;y>=0;y--)for(int x=0;x<336;x++) {
        unsigned v=MD_Screen[y*336+x];
        unsigned char rgb[3]={(unsigned char)((v&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v>>11)*255/31)};
        fwrite(rgb,1,3,f); if(v)lit++;
    }
    int ok=!ferror(f) && lit>4000; fclose(f); return ok;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2 && argc<=4);
    const int fast=argc>=3 && !strcmp(argv[2],"fast");
    const int accurate=argc>=3 && !strcmp(argv[2],"accurate");
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
    _putenv("GENS_HLE_TRACE=trace.log"); _putenv("GENS_HLE_SAVE_DIR=.");
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0;
    _32X_Genesis_Bios[0]=_32X_Master_Bios[0]=_32X_Slave_Bios[0]=0;
    RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100;
    Recalculate_Palettes();
    Sound_Rate=argc==4?atoi(argv[3]):44100;
    CHECK(Sound_Rate==11025||Sound_Rate==22050||Sound_Rate==44100);
    Sound_Enable=0; Country=0; SegaCD_Accurate=accurate;
    CDDA_Enable=PWM_Enable=YM2612_Enable=PSG_Enable=1;
    Seg_Lenght=(Sound_Rate+30)/60;
    for(int i=0;i<262;i++) {
        Sound_Extrapol[i][0]=Seg_Lenght*i/262;
        Sound_Extrapol[i][1]=Seg_Lenght*(i+1)/262-Sound_Extrapol[i][0];
    }
    for(int i=0;i<Seg_Lenght;i++)Sound_Interpol[i]=262*i/Seg_Lenght;
    Z80_State=1; MSH2_Speed=SSH2_Speed=100;
    MSH2_Init(); SSH2_Init(); M68K_Init(); S68K_Init(); Z80_Init(); Init_Tab(); FILE_Init();
    CHECK(boot_contract_tests()==0);
    CHECK(dma_tests()==0);
    CHECK(Detect_Format(argv[1])==SEGACD_IMAGE);
    release_pad(); SegaCD_Started=Init_SegaCD(argv[1]);
    CHECK(SegaCD_Started && CD_HLE_Active && !CD_32X_Active);
    unsigned audio=0, before=0, after=0;
    FILE *pcm=fopen("playback.pcm","wb"); CHECK(pcm);
    for(int i=0;i<3600;i++) {
        release_pad();
        // MODS -> 6_8CH -> D2.MOD, using real controller reads.
        Controller_1_Down=((i>=180&&i<192)||(i>=370&&i<376)||(i>=490&&i<496))?0:1;
        Controller_1_A=(pulse(i,300)||pulse(i,420)||pulse(i,540))?0:1;
        memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
        if(fast&&i%60!=59)Update_Frame_Fast();else Update_Frame();
        CHECK(!Paused);
        if(i>900) {
            for(int n=1;n<Seg_Lenght;n++)if(Seg_L[n]!=Seg_L[0]||Seg_R[n]!=Seg_R[0]) { audio++; break; }
            if(i<1200)for(int n=0;n<Seg_Lenght;n++) {
                short pair[2];
                for(int ch=0;ch<2;ch++) { int v=(ch?Seg_R:Seg_L)[n]; if(v>32767)v=32767;if(v<-32768)v=-32768;pair[ch]=(short)v; }
                fwrite(pair,2,2,pcm);
            }
        }
        if(i==150) {
            CHECK(CD_32X_Active && _32X_ADEN && _32X_RES);
            CHECK(!_32X_Started);
            CHECK(GetMenuState(Gens_Menu,ID_CPU_DEBUG_32X_VDP,MF_BYCOMMAND)!=0xffffffff);
            CHECK(GetMenuState(Gens_Menu,ID_CPU_DEBUG_SEGACD_68000,MF_BYCOMMAND)!=0xffffffff);
            CHECK(live_vdp_test()==0);
            screenshot("menu.bmp");
        }
        if(i==959) { CHECK(screenshot("playing.bmp")); before=hash((unsigned char*)MD_Screen,336*240*2); }
        if(i==1019) { CHECK(screenshot("playing-later.bmp")); after=hash((unsigned char*)MD_Screen,336*240*2); }
        if(i==1200) {
            // Changing CD timing preferences must keep the combined scheduler.
            CHECK(Do_SegaCD_Frame_Cycle_Accurate());
            CHECK(Do_SegaCD_Frame_No_VDP_Cycle_Accurate());
        }
    }
    CHECK(!ferror(pcm)); fclose(pcm);
    CHECK(audio>2400 && before!=after);
    CHECK((SH2_Get_PC(&M_SH2)&0xfffc0000)==0x06000000);
    CHECK((SH2_Get_PC(&S_SH2)&0xfffc0000)==0x06000000);
    CHECK(!Save_State("unsupported.gs0") && !Load_State("unsupported.gs0"));
    release_pad(); CHECK(Reload_SegaCD(argv[1])); CHECK(!CD_32X_Active);
    for(int i=0;i<180;i++)Update_Frame();
    CHECK(CD_32X_Active && _32X_ADEN && !Paused);
    screenshot("reset.bmp");
    Free_Rom(Game); CHECK(!CD_32X_Active && !CD_HLE_Active && !SegaCD_Started);
    printf("PASS: ISO boot, input, animated 32X display, %u varying audio frames, reset, close; %s/%d Hz\n",audio,argc>=3?argv[2]:"normal",Sound_Rate);
    return 0;
}
