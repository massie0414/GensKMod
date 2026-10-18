// SEGA CLASSIC integration test. The user supplies the disc; no game/BIOS bytes are embedded.
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
#include "../src/Gens/Mem_Z80.h"
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
    unsigned lit=0, colors=0; unsigned char seen[65536]={0};
    for(int y=239;y>=0;y--)for(int x=0;x<336;x++) {
        unsigned v=MD_Screen[y*336+x];
        unsigned char rgb[3]={(unsigned char)((v&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v>>11)*255/31)};
        fwrite(rgb,1,3,f); if(v)lit++; if(!seen[v]) { seen[v]=1; colors++; }
    }
    int ok=!ferror(f); fclose(f); return ok && lit>200 && colors>=2;
}

static unsigned picture_hash(void) {
    unsigned h=2166136261U;
    for(unsigned i=0;i<336*240;i++)h=(h^MD_Screen[i])*16777619U;
    return h;
}
int main(int argc,char **argv) {
    CHECK(argc==5);
    const int opening=!strcmp(argv[2],"opening");
    const int game=!strcmp(argv[2],"streets")?1:!strcmp(argv[2],"columns")?2:!strcmp(argv[2],"shinobi")?3:0;
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
    YM2612_Enable=PSG_Enable=1;
    YM2612_Init(CLOCK_NTSC/7,Sound_Rate,0); PSG_Init(CLOCK_NTSC/15,Sound_Rate);
    M68K_Init(); S68K_Init(); Z80_Init(); Init_Tab(); FILE_Init();
    CHECK(Detect_Format(argv[1])==SEGACD_IMAGE+1);
    SegaCD_Started=Init_SegaCD(argv[1]);
    CHECK(SegaCD_Started && CD_HLE_Active);

    unsigned audio_frames=0, cdda_frames=0, title_hash=0, stage_hash=0, motion_hash=0;
    for(int i=0;i<12000;i++) {
        Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
        Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_C=1;
        Controller_1_Mode=Controller_1_X=Controller_1_Y=Controller_1_Z=1;
        if(!opening) {
            if((game&2) && i>=800 && i<812)Controller_1_Down=0;
            if((game&1) && i>=820 && i<832)Controller_1_Right=0;
            if((i>=900 && i<912)||(i>=1200 && i<1212)||(i>=1600 && i<1612))Controller_1_Start=0;
            if(game==3 && ((i>=3000 && i<3012)||(i>=3600 && i<3612)))Controller_1_Start=0;
            if(i>=2000 && i%300<12)Controller_1_C=0;
            if(i>=4000 && i<5000)Controller_1_Right=0;
        }
        memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
        if(accurate)Do_SegaCD_Frame_Cycle_Accurate(); else Do_SegaCD_Frame();
        if(Paused || main68k_context.pc==0x1800) { fprintf(stderr,"PAUSED frame=%d main=%x sub=%x\n",i,main68k_context.pc,sub68k_context.pc); return 2; }
        for(int j=0;j<Seg_Lenght;j++)
            if(Seg_L[j]||Seg_R[j]) { audio_frames++; if(CD_HLE_AudioPlaying())cdda_frames++; break; }
        if(i%600==0)CHECK(HeapValidate(GetProcessHeap(),0,NULL));
        if(i==1199 || i==2399 || i==5999 || i==11999) {
            char name[80]; sprintf(name,"%s-%05d.bmp",opening?"opening":"play",i+1);
            int visible=screenshot(name); if(i>=2399)CHECK(visible); printf("frame=%d main=%x sub=%x\n",i+1,main68k_context.pc,sub68k_context.pc); fflush(stdout);
        }
        if(i==1199)title_hash=picture_hash();
        if(i==5999)motion_hash=picture_hash();
        if(i==11999)stage_hash=picture_hash();
    }
    CHECK(audio_frames>1000 && title_hash!=stage_hash);
    if(game==0 && !opening)CHECK(motion_hash!=stage_hash);
    for(int i=0;i<120;i++) { Do_SegaCD_Frame_No_VDP(); if(Paused || main68k_context.pc==0x1800) { fprintf(stderr,"PAUSED frame=%d main=%x sub=%x\n",i,main68k_context.pc,sub68k_context.pc); return 2; } }
    CopyFile("trace.log","completed-trace.log",FALSE);
    Reset_SegaCD(); CHECK(CD_HLE_Active && !Paused);
    CHECK(main68k_context.pc==0xff0000 && sub68k_context.pc==0x1000);
    Free_Rom(Game); FILE_End(); CHECK(!CD_HLE_Active);
    printf("PASS: SEGA CLASSIC %s, %s timing, %d Hz, 12000 frames, %u audible frames (%u CDDA); reset and teardown.\n",
        argv[2],accurate?"accurate":"default",Sound_Rate,audio_frames,cdda_frames);
    return 0;
}


