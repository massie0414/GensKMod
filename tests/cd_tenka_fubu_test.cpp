// TENKA FUBU integration test. The user supplies the disc; no game/BIOS bytes are embedded.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/Gens/gens.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_Z80.h"
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
#include "../src/Gens/pcm.h"



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
    unsigned lit=0, colors=0; unsigned char seen[65536]={0};
    for(int y=239;y>=0;y--)for(int x=0;x<336;x++) {
        unsigned v=MD_Screen[y*336+x];
        unsigned char rgb[3]={(unsigned char)((v&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v>>11)*255/31)};
        fwrite(rgb,1,3,f); if(v)lit++; if(!seen[v]) { seen[v]=1; colors++; }
    }
    int ok=!ferror(f); fclose(f); return ok && lit>1000 && colors>=8;
}

static unsigned picture_hash(void) {
    unsigned h=2166136261U;
    for(unsigned i=0;i<336*240;i++)h=(h^MD_Screen[i])*16777619U;
    return h;
}
int main(int argc,char **argv) {
    CHECK(argc==5);
    const int opening=!strcmp(argv[2],"opening");
    const int accurate=!strcmp(argv[3],"accurate");
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
    _putenv("GENS_HLE_TRACE=trace.log"); _putenv("GENS_HLE_SAVE_DIR=.");
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0;
    RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100;
    Recalculate_Palettes();
    Sound_Rate=atoi(argv[4]); CHECK(Sound_Rate==22050 || Sound_Rate==44100);
    Sound_Enable=0; Country=0; CDDA_Enable=1; PCM_Enable=1; Z80_State=1;
    Seg_Lenght=(Sound_Rate+30)/60;
    for(int i=0;i<262;i++) {
        Sound_Extrapol[i][0]=Seg_Lenght*i/262;
        Sound_Extrapol[i][1]=Seg_Lenght*(i+1)/262-Sound_Extrapol[i][0];
    }
    for(int i=0;i<Seg_Lenght;i++)Sound_Interpol[i]=262*i/Seg_Lenght;
    M68K_Init(); S68K_Init(); Z80_Init(); Init_Tab(); FILE_Init();
    CHECK(Detect_Format(argv[1])==SEGACD_IMAGE+1);
    SegaCD_Started=Init_SegaCD(argv[1]);
    CHECK(SegaCD_Started && CD_HLE_Active && SCD.TOC.Last_Track==19);
    unsigned audio_frames=0, cdda_frames=0, title_hash=0, stage_hash=0;
    for(int i=0;i<30000;i++) {
        Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
        Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_C=1;
        Controller_1_Mode=Controller_1_X=Controller_1_Y=Controller_1_Z=1;
        if(!opening) {
            if((i>=11850 && i<11862) || (i>=12250 && i<12262))Controller_1_Start=0;
            if(i>=13100 && i<13112)Controller_1_Left=0;
            if(i>=13200 && i<13212)Controller_1_Start=0;
            if(i>=13600 && i<13612)Controller_1_A=0;
            if(i>=14500 && i<24500 && i%1000<12)Controller_1_Start=0;
            if((i>=17500 && i<17512)||(i>=18500 && i<18512)||(i>=20500 && i<20512))Controller_1_C=0;
        }
        memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
        if(accurate)Do_SegaCD_Frame_Cycle_Accurate(); else Do_SegaCD_Frame();
        if(Paused) { fprintf(stderr,"PAUSED frame=%d main=%x sub=%x\n",i,main68k_context.pc,sub68k_context.pc); return 2; }
        for(int j=0;j<Seg_Lenght;j++)
            if(Seg_L[j]||Seg_R[j]) { audio_frames++; if(CD_HLE_AudioPlaying())cdda_frames++; break; }
        if(i%600==0)CHECK(HeapValidate(GetProcessHeap(),0,NULL));
        if(i==399 || i==5999 || (!opening && (i==12999 || i==13999 || i==15999 || i==17999 || i==23999))) {
            char name[80]; sprintf(name,"%s-%05d.bmp",opening?"opening":"play",i+1);
            CHECK(screenshot(name));
        }
        if(i==12999)title_hash=picture_hash();
        if(i==23999) {
            stage_hash=picture_hash();
            if(!opening)CHECK((VDP_Reg.Set2&0x40) && VDP_Reg.Pat_ScrA_Adr==0x30);
        }
    }
    CHECK(audio_frames>5000 && cdda_frames>300 && title_hash!=stage_hash);
    for(int i=0;i<120;i++) { Do_SegaCD_Frame_No_VDP(); if(Paused) { fprintf(stderr,"PAUSED frame=%d main=%x sub=%x\n",i,main68k_context.pc,sub68k_context.pc); return 2; } }
    Reset_SegaCD(); CHECK(CD_HLE_Active && !Paused);
    CHECK(main68k_context.pc==0xff0000 && sub68k_context.pc==0x1000);
    Free_Rom(Game); FILE_End(); CHECK(!CD_HLE_Active);
    printf("PASS: TENKA FUBU %s, %s timing, %d Hz, 30000 frames, %u audible frames (%u CDDA); reset and teardown.\n",
        opening?"opening/title/demo":"scenario/daimyo selection and campaign map",accurate?"accurate":"default",Sound_Rate,audio_frames,cdda_frames);
    return 0;
}
