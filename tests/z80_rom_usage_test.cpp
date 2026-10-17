#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/Gens/Mem_Z80.h"
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

int main(int argc,char **argv) {
 CHECK(argc==2 || argc==3);
 GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
 RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100; Recalculate_Palettes();
 Sound_Rate=44100; Sound_Enable=0; Country=0; YM2612_Enable=1; PSG_Enable=1;
 Seg_Lenght=(Sound_Rate+30)/60;
 for(int i=0;i<262;i++) { Sound_Extrapol[i][0]=Seg_Lenght*i/262; Sound_Extrapol[i][1]=Seg_Lenght*(i+1)/262-Sound_Extrapol[i][0]; }
 for(int i=0;i<Seg_Lenght;i++)Sound_Interpol[i]=262*i/Seg_Lenght;
 Z80_State=1; M68K_Init(); Z80_Init(); Init_Tab(); Game=Load_Rom(NULL,argv[1],0); CHECK(Game);
 Genesis_Started=Init_Genesis(Game); CHECK(Genesis_Started);
 unsigned audio=0, soundHash=2166136261u, ramHash=2166136261u, silentChecks=0; unsigned long usage[256];
 for(int frame=0;frame<1800;frame++) {
  Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
  Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_C=1;
  if(frame>=900 && frame<906)Controller_1_Start=0;
  memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R)); Do_Genesis_Frame_No_VDP();
  for(int j=0;j<Seg_Lenght;j++)if(Seg_L[j]!=Seg_L[0] || Seg_R[j]!=Seg_R[0]) { audio++; break; }
  for(int j=0;j<Seg_Lenght;j++) { soundHash=(soundHash^(unsigned)Seg_L[j])*16777619u; soundHash=(soundHash^(unsigned)Seg_R[j])*16777619u; }
  for(unsigned j=0;j<8192;j++)ramHash=(ramHash^Ram_Z80[j])*16777619u;
  for(unsigned j=0;j<65536;j++)ramHash=(ramHash^Ram_68k[j])*16777619u;
  if(frame%60==59) {
   unsigned count=Z80_Usage_GetHistory(usage), pc=z80_Get_PC(&M_Z80);
   if(frame>=119 && !audio && argc==2) { CHECK(count && usage[count-1]<200); ++silentChecks; }
   printf("frame=%d audioFrames=%u usage=%lu pc=%04x state=%u loop=%x bytes=",frame+1,audio,count?usage[count-1]:9999,pc,Z80_State,Z80_Polling_Loop(&M_Z80));
   for(unsigned j=0;j<20;j++)printf("%02x",Ram_Z80[(pc+j)&8191]); puts(""); fflush(stdout); audio=0;
  }

 }
 printf("RESULT soundHash=%08x ramHash=%08x silentChecks=%u\n",soundHash,ramHash,silentChecks);
 return 0;
}
