#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/Gens/gens.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_SH2.h"
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/Mem_SH2.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/vdp_32X.h"
#include "../src/Gens/G_dsound.h"
#include "../src/Gens/rom.h"
#include "../src/Gens/save.h"
#include "../src/Gens/vdp_rend.h"
#include "../src/Gens/io.h"
#include "../src/Gens/32x_boot.h"
#include "../src/Gens/pwm.h"
#include "../src/Gens/ym2612.h"
#include "../src/Gens/psg.h"
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#x); return 1; } } while(0)
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

static unsigned hash(const void *data, unsigned size)
{
    const unsigned char *p=(const unsigned char *)data;
    unsigned h=2166136261u;
    while(size--)h=(h ^ *p++)*16777619u;
    return h;
}
static int boot_files(void)
{
    CHECK(Load_32X_Boot(Rom_Data,Rom_Size) && Boot_32X_Internal);
    unsigned char raw[256];
    for(unsigned i=0;i<256;i++)raw[i]=_32X_Genesis_Rom[i^1];
    const char *names[]={"test-g.bin","test-m.bin","test-s.bin"};
    const unsigned char *buffers[]={raw,_32X_MSH2_Rom,_32X_SSH2_Rom};
    const unsigned sizes[]={256,2048,1024};
    for(int i=0;i<3;i++) {
        FILE *f=fopen(names[i],"wb"); CHECK(f);
        CHECK(fwrite(buffers[i],1,sizes[i],f)==sizes[i]); fclose(f);
    }
    strcpy(_32X_Genesis_Bios,names[0]); strcpy(_32X_Master_Bios,names[1]); strcpy(_32X_Slave_Bios,names[2]);
    unsigned g=hash(_32X_Genesis_Rom,256),m=hash(_32X_MSH2_Rom,2048),sl=hash(_32X_SSH2_Rom,1024);
    CHECK(Load_32X_Boot(Rom_Data,Rom_Size) && !Boot_32X_Internal);
    CHECK(g==hash(_32X_Genesis_Rom,256) && m==hash(_32X_MSH2_Rom,2048) && sl==hash(_32X_SSH2_Rom,1024));
    // A short file must never leave stale/partly loaded external boot programs.
    FILE *f=fopen(names[1],"wb"); CHECK(f); fputc(0,f); fclose(f);
    CHECK(Load_32X_Boot(Rom_Data,Rom_Size) && Boot_32X_Internal);
    CHECK(g==hash(_32X_Genesis_Rom,256) && m==hash(_32X_MSH2_Rom,2048) && sl==hash(_32X_SSH2_Rom,1024));
    _32X_Genesis_Bios[0]=_32X_Master_Bios[0]=_32X_Slave_Bios[0]=0;
    unsigned char header[0x400]={0};
    CHECK(!Load_32X_Boot(header,0x3ff));
    header[0x3d4]=0xff; CHECK(!Load_32X_Boot(header,sizeof(header))); header[0x3d4]=0;
    header[0x3d8]=0xff; CHECK(!Load_32X_Boot(header,sizeof(header))); header[0x3d8]=0;
    header[0x3dc]=0xff; CHECK(!Load_32X_Boot(header,sizeof(header))); header[0x3dc]=0;
    CHECK(Load_32X_Boot(header,sizeof(header))); // empty initial load
    CHECK(Load_32X_Boot(Rom_Data,Rom_Size));
    puts("PASS: external set, truncated fallback, invalid ranges and empty initial load");
    return 0;
}
static void release_pad(void)
{
    Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
    Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_C=1;
    Controller_1_Mode=Controller_1_X=Controller_1_Y=Controller_1_Z=1;
}
static int checkpoint(char *name)
{
    unsigned ram=hash(_32X_Ram,sizeof(_32X_Ram)), pc=SH2_Get_PC(&M_SH2), spc=SH2_Get_PC(&S_SH2);
    CHECK(Save_State(name));
    for(int i=0;i<10;i++)Do_32X_Frame();
    CHECK(Load_State(name));
    CHECK(ram==hash(_32X_Ram,sizeof(_32X_Ram)) && pc==SH2_Get_PC(&M_SH2) && spc==SH2_Get_PC(&S_SH2));
    return 0;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2 && argc<=3);
    int fast=argc==3 && !strcmp(argv[2],"fast");
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
    RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100;
    Recalculate_Palettes();
    Sound_Rate=44100; Sound_Enable=0; Country=0;
    PWM_Enable=1; YM2612_Enable=1; PSG_Enable=1;
    Seg_Lenght=(Sound_Rate+30)/60;
    for(int i=0;i<262;i++) {
        Sound_Extrapol[i][0]=Seg_Lenght*i/262;
        Sound_Extrapol[i][1]=Seg_Lenght*(i+1)/262-Sound_Extrapol[i][0];
    }
    for(int i=0;i<Seg_Lenght;i++)Sound_Interpol[i]=262*i/Seg_Lenght;
    Z80_State=1; MSH2_Speed=SSH2_Speed=100;
    M68K_Init(); Z80_Init(); MSH2_Init(); SSH2_Init(); Init_Tab();
    Game=Load_Rom(NULL,argv[1],0); CHECK(Game);
    CHECK(boot_files()==0);
    _32X_Started=Init_32X(Game); CHECK(_32X_Started && Boot_32X_Internal);
    CHECK(checkpoint("boot.gs0")==0);
    unsigned audio=0, active=0, before_move=0, after_move=0;
    for(int i=0;i<6000;i++) {
        release_pad();
        if((i>=1800 && i<1806)||(i>=2100 && i<2106)||
           (i>=2400 && i<2406)||(i>=3000 && i<3006))Controller_1_Start=0;
        if(i>=3600 && i<4200)Controller_1_Up=0;
        if(i>=4200 && i<4500)Controller_1_Right=0;
        if(i>=4500 && i<4800)Controller_1_B=0;
        memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
        if(fast && i%60!=59)Do_32X_Frame_No_VDP(); else Do_32X_Frame();
        for(int j=0;j<Seg_Lenght;j++) if(Seg_L[j]!=Seg_L[0] || Seg_R[j]!=Seg_R[0]) { audio++; break; }
        if(_32X_VDP.Mode&3)active++;
        if(i==3599)before_move=hash(_32X_VDP_Ram,256*1024);
        if(i==4199)after_move=hash(_32X_VDP_Ram,256*1024);
        if(i==4999)CHECK(checkpoint("mission.gs0")==0);
        if(i%600==599) {
            char path[64]; sprintf(path,"frame-%04d.bmp",i+1); CHECK(screenshot(path));
            printf("frame %d: 68k=%08x master=%08x slave=%08x\n",i+1,main68k_context.pc,SH2_Get_PC(&M_SH2),SH2_Get_PC(&S_SH2)); fflush(stdout);
        }
    }
    CHECK(active>3000 && audio>1000 && before_move!=after_move);
    CHECK((SH2_Get_PC(&M_SH2)&0xfffc0000)==0x06000000);
    CHECK((SH2_Get_PC(&S_SH2)&0xfffc0000)==0x06000000);
    release_pad(); Reset_32X();
    CHECK(!_32X_ADEN && !_32X_RES && SH2_Get_PC(&M_SH2)==0x204);
    for(int i=0;i<1800;i++)Do_32X_Frame();
    CHECK(_32X_ADEN && _32X_RES && (_32X_VDP.Mode&3));
    CHECK(screenshot("reset.bmp"));
    CHECK((SH2_Get_PC(&M_SH2)&0xfffc0000)==0x06000000);
    printf("PASS: %u audio frames; game input, boot/mission states, reset, %s rendering\n",audio,fast?"frame skip":"normal");
    Free_Rom(Game);
    return 0;
}
