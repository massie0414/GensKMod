// Optional integration test for CD32X_Template; no disc or BIOS data embedded.
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


#include <assert.h>
#define CHECK(x) assert(x)
int main(int argc,char **argv) {
 if(argc<2 || argc>3)return 2;
 const int accurate=0;
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
    _putenv("GENS_HLE_TRACE=trace.log"); _putenv("GENS_HLE_SAVE_DIR=.");
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0;
    _32X_Genesis_Bios[0]=_32X_Master_Bios[0]=_32X_Slave_Bios[0]=0;
    RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100;
    Recalculate_Palettes();
    Sound_Rate=44100;
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

 Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
 Controller_1_A=Controller_1_B=Controller_1_C=Controller_1_Start=1;
 SegaCD_Started=Init_SegaCD(argv[1]);
 if(!SegaCD_Started)return 3;
 unsigned long titleTotal=0, titleCount=0, gameTotal=0, gameCount=0;
 unsigned long titleMin=1000,titleMax=0,gameMin=1000,gameMax=0;
 for(int i=0;i<1200;++i) {
  Controller_1_Start=(i>=600 && i<612)?0:1;
  memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
  if(argc>2 && !strcmp(argv[2],"fast"))Update_Frame_Fast(); else Update_Frame();
  if(i>=120 && i<600) {
   CHECK(CD_32X_Active && !_32X_Started);
   CHECK(M68K_RW(0xa15120)==0);
   CHECK(M_SH2.Usage_PerMille>=1 && M_SH2.Usage_PerMille<=1001);
   unsigned usage=M_SH2.Usage_PerMille-1;
   titleTotal+=usage;++titleCount;
   if(usage<titleMin)titleMin=usage;if(usage>titleMax)titleMax=usage;
   CHECK(S_SH2.Usage_PerMille<50);
  }
  if(i>=840) {
   CHECK(M68K_RW(0xa15120)==1);
   CHECK(M_SH2.Usage_PerMille>=1 && M_SH2.Usage_PerMille<=1001);
   unsigned usage=M_SH2.Usage_PerMille-1;
   gameTotal+=usage;++gameCount;
   if(usage<gameMin)gameMin=usage;if(usage>gameMax)gameMax=usage;
  }
  if(i%120==119)printf("frame=%d pc=%08lx usage=%.1f%% idle=%lu total=%lu show=%x\n",i,SH2_Get_PC(&M_SH2),(M_SH2.Usage_PerMille-1)/10.0,M_SH2.Idle_Cycles,M_SH2.Odometer,M68K_RW(0xa15120));
 }
 printf("Title mean=%.1f%% range=%.1f..%.1f%%; game mean=%.1f%% range=%.1f..%.1f%%\n",titleTotal/(10.0*titleCount),titleMin/10.0,titleMax/10.0,gameTotal/(10.0*gameCount),gameMin/10.0,gameMax/10.0);
 // The template clears its framebuffer even on the title screen: this is
 // real work, but the VBlank wait must not make it appear fully busy.
 CHECK(titleTotal/titleCount>100 && titleTotal/titleCount<900);
 CHECK(gameTotal/gameCount>100 && gameTotal/gameCount<950);
 CHECK(M68K_RW(0xa15120)==1);
 puts("PASS: real CD32X template title/game usage and inactive slave");
 return 0;
}
