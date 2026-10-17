// Synthetic disc tests: no game or firmware data is embedded.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/Gens/gens.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Mem_M68k.h"
#include "../src/Gens/Mem_S68k.h"
#include "../src/Gens/cd_file.h"
#include "../src/Gens/cd_hle.h"
#include "../src/Gens/cd_sys.h"
#include "../src/Gens/G_dsound.h"
#include "../src/Gens/vdp_io.h"

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void be32(unsigned char *p,unsigned v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void word(unsigned char *p,unsigned a,unsigned v) { p[a^1]=v>>8; p[(a+1)^1]=v; }
static unsigned rw(unsigned char *p,unsigned a) { return p[a^1]*256+p[(a+1)^1]; }
static void longword(unsigned char *p,unsigned a,unsigned v) { word(p,a,v>>16); word(p,a+2,v); }
static S68000CONTEXT api(unsigned fn,unsigned a0=0,unsigned a1=0,unsigned d1=0) {
    S68000CONTEXT saved=sub68k_context;
    sub68k_context.pc=0x1206; sub68k_context.interrupts[0]=1;
    sub68k_context.areg[7]=0x5600; sub68k_context.areg[0]=a0; sub68k_context.areg[1]=a1;
    sub68k_context.dreg[0]=fn; sub68k_context.dreg[1]=d1;
    word(Ram_Prg,0x5600,0x2010); longword(Ram_Prg,0x5602,0x1400);
    CD_HLE_SubExec((int)sub68k_context.odometer);
    S68000CONTEXT result=sub68k_context; sub68k_context=saved; return result;
}
int main(void) {
    unsigned char data[64*2048]={0}, raw[2352];
    memcpy(data,"SEGADISCSYSTEM",14);
    be32(data+0x30,0x200); be32(data+0x34,4);
    be32(data+0x40,0x800); be32(data+0x44,0x40);
    data[0x200]=0x60; data[0x201]=0xfe;
    be32(data+0x818,0x20);
    data[0x821]=0x10; data[0x823]=0x12; data[0x825]=0x14; data[0x827]=0x16;
    data[0x830]=0x4e; data[0x831]=0x75;
    data[0x832]=0x60; data[0x833]=0xfe;
    data[0x834]=data[0x836]=0x4e; data[0x835]=data[0x837]=0x75;
    for(unsigned i=0;i<2048;i++)data[33*2048+i]=(i*17+i/256)&255;
    FILE *f=fopen("synthetic.bin","wb"); CHECK(f);
    for(unsigned i=0;i<64;i++) {
        memset(raw,0xe5,sizeof(raw)); memcpy(raw+16,data+i*2048,2048);
        CHECK(fwrite(raw,1,sizeof(raw),f)==sizeof(raw));
    }
    fclose(f); f=fopen("synthetic.cue","w"); CHECK(f);
    fprintf(f,"FILE \"synthetic.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n");
    for(unsigned t=2;t<=22;t++)fprintf(f," TRACK %02u AUDIO\n%s INDEX 01 00:00:%02u\n",t,t==2?" PREGAP 00:02:00\n":"",38+t);
    fclose(f);
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0;
    Sound_Enable=0; Sound_Rate=44100; Country=0;
    M68K_Init(); S68K_Init(); Z80_Init(); Init_Tab(); FILE_Init();
    SegaCD_Started=Init_SegaCD("synthetic.cue"); CHECK(SegaCD_Started && CD_HLE_Active);
    CHECK(Ram_Word_State==2 && SCD.TOC.Last_Track==22);
    // Software TRAPs must reach hooks installed after boot, on both CPUs.
    for(int sub=0;sub<2;sub++)for(unsigned n=0;n<16;n++) {
        unsigned char *p=sub?Ram_Prg:Ram_68k;
        unsigned hook=(sub?0x5fa0:0xfd18)+n*6;
        unsigned base=sub?0:0xff0000;
        word(p,hook,0x4ef9); longword(p,hook+2,base+0x5500);
        word(p,0x5500,0x7000|n); word(p,0x5502,0x4e73);
        word(p,0x5400,0x4e40+n); word(p,0x5402,0x60fe);
        S68000CONTEXT *c=sub?&sub68k_context:&main68k_context, saved=*c;
        c->pc=base+0x5400; c->areg[7]=base+0x5700; c->sr=0x2700; c->dreg[0]=0x1234;
        if(sub) { sub68k_tripOdometer(); CD_HLE_SubExec(100); }
        else { main68k_tripOdometer(); CD_HLE_MainExec(100); }
        CHECK(c->pc==base+0x5402 && c->areg[7]==base+0x5700 && c->dreg[0]==n);
        *c=saved;
    }
    // Replacing USER call 2 must replace the boot-header interrupt target.
    word(Ram_Prg,0x5f34,0x4ef9); longword(Ram_Prg,0x5f36,0x5500);
    word(Ram_Prg,0x5500,0x5279); longword(Ram_Prg,0x5502,0x5900); word(Ram_Prg,0x5506,0x4e75);
    sub68k_context.pc=0x6032; sub68k_context.areg[7]=0x5700; sub68k_context.sr=0x2000;
    sub68k_tripOdometer(); sub68k_interrupt(2,-1); CD_HLE_SubExec(500);
    CHECK(rw(Ram_Prg,0x5900)==1);
    // TOC times are BCD, but track numbers are binary, including tracks 10..22.
    S68000CONTEXT r=api(0x83,0,0,1); CHECK(r.dreg[0]==0x00020001 && (r.dreg[1]&255)==255);
    r=api(0x83,0,0,10); CHECK(r.dreg[0]==0x0004480a && !(r.dreg[1]&255));
    r=api(0x83,0,0,22); CHECK(r.dreg[0]==0x00046016 && !(r.dreg[1]&255));
    r=api(0x83,0,0,99); CHECK(r.dreg[0]==0x00020000);
    r=api(0x81); CHECK(r.areg[0]==0x1300 && rw(Ram_Prg,0x1310)==0x0116);
    // Adjacent Mode 1 reads advance by 2048 and never copy the EDC/ECC tail.
    longword(Ram_Prg,0x5200,33); longword(Ram_Prg,0x5204,1);
    r=api(0x20,0x5200); CHECK(!(r.sr&1));
    memset(Ram_Prg+0x20000,0xa5,0x1000);
    r=api(0x8c,0x20000,0x5300);
    CHECK(!(r.sr&1) && r.areg[0]==0x20800 && r.areg[1]==0x5304);
    for(unsigned i=0;i<2048;i++)CHECK(Ram_Prg[(0x20000+i)^1]==data[33*2048+i]);
    CHECK(rw(Ram_Prg,0x20800)==0xa5a5);
    // The same API must write the initial sub-CPU 1M bank at $0C0000.
    r=api(0x8c,0xc0000,0x5300); CHECK(!(r.sr&1));
    for(unsigned i=0;i<2048;i++)CHECK(Ram_Word_1M[(0x20000+i)^1]==data[33*2048+i]);
    Free_Rom(Game); FILE_End(); CHECK(!CD_HLE_Active);
    puts("PASS: HLE writable TRAP/USER hooks, TOC, Mode 1 transfers and initial 1M banks");
    return 0;
}
