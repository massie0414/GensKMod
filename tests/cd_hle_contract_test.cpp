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
#include "../src/Gens/LC89510.h"

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
static S68000CONTEXT main_api(unsigned fn,unsigned a0=0,unsigned a1=0,unsigned d0=0,unsigned d1=0,unsigned d2=0) {
    S68000CONTEXT saved=main68k_context;
    main68k_context.pc=0x2006+(fn-0x200)*2; main68k_context.interrupts[0]=0x10;
    main68k_context.areg[7]=0xff5600; main68k_context.areg[0]=a0; main68k_context.areg[1]=a1;
    main68k_context.dreg[0]=d0; main68k_context.dreg[1]=d1; main68k_context.dreg[2]=d2;
    word(Ram_68k,0x5600,0x2010); longword(Ram_68k,0x5602,0xff5400);
    CD_HLE_MainExec((int)main68k_context.odometer);
    S68000CONTEXT result=main68k_context; main68k_context=saved; return result;
}
int main(void) {
    unsigned char data[64*2048]={0}, raw[2352];
    memcpy(data,"SEGADISCSYSTEM",14);
    be32(data+0x30,0x800); be32(data+0x34,0x40);
    be32(data+0x40,0x800); be32(data+0x44,0x40);
    data[0x83e]=0xbe; data[0x83f]=0xef;
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
    CHECK(rw(Ram_Prg,0x5e90)==0x0116); // Direct BIOS TOC cache access.
    CHECK(rw(Ram_68k,0xfdb4)==0x8004 && rw(Ram_68k,0xfdb6)==0x8124);
    CHECK(rw(Ram_68k,0x63e)==0xbeef); // IP length excludes the $600-byte security block.
    CHECK(rw(Ram_68k,0xfda8)==0x4ef9); // Games patch the callback target only.
    // _MBURAM probes an optional RAM cartridge, not internal sub-CPU BRAM.
    // Execute a real absolute-short JSR as games do (sign-extended $FDAE).
    CHECK(rw(Ram_68k,0xfdae)==0x4ef9);
    for(unsigned fn=0;fn<=8;fn++) {
        S68000CONTEXT saved=main68k_context;
        word(Ram_68k,0x5400,0x4eb8); word(Ram_68k,0x5402,0xfdae);
        word(Ram_68k,0x5404,0x60fe);
        main68k_context.pc=0xff5400; main68k_context.areg[7]=0xff5700;
        main68k_context.sr=0x2700; main68k_context.dreg[0]=fn;
        main68k_context.dreg[1]=0xdeadbeef;
        main68k_context.areg[0]=0x201250; main68k_context.areg[1]=0x201ed0;
        main68k_tripOdometer(); CD_HLE_MainExec(100);
        CHECK(main68k_context.pc==0xff5404 && main68k_context.areg[7]==0xff5700);
        CHECK((main68k_context.sr&1) && main68k_context.dreg[1]==0);
        CHECK(main68k_context.dreg[0]==fn);
        CHECK(main68k_context.areg[0]==0x201250 && main68k_context.areg[1]==0x201ed0);
        CHECK(rw(Ram_68k,0xfdb4)==0x8004); // Adjacent VDP shadow is intact.
        main68k_context=saved;
    }
    // Default V-blank user callback is callable, and can be replaced by the game.
    longword(Ram_68k,0xfdaa,0xff5500);
    word(Ram_68k,0x5500,0x7007); word(Ram_68k,0x5502,0x4e75);
    word(Ram_68k,0x5400,0x4eb8); word(Ram_68k,0x5402,0xfda8); word(Ram_68k,0x5404,0x60fe);
    { S68000CONTEXT saved=main68k_context;
      main68k_context.pc=0xff5400; main68k_context.areg[7]=0xff5700; main68k_context.sr=0x2700;
      main68k_tripOdometer(); CD_HLE_MainExec(100);
      CHECK(main68k_context.dreg[0]==7 && main68k_context.areg[7]==0xff5700);
      main68k_context=saved; }
    // Prefix-table and inline Nemesis runs, including XOR rows and exact RAM bounds.
    { const unsigned char stream[]={0x80,1,0x8a,0x71,0,0xff,0x7f,0xe8,0};
      for(unsigned i=0;i<sizeof(stream);i++)Ram_68k[(0x5000+i)^1]=stream[i];
      memset(Ram_68k+0x5100,0xa5,36);
      unsigned saved_a2=main68k_context.areg[2];
      main68k_context.areg[2]=0xff5100;
      S68000CONTEXT decoded=main_api(0x2f0,0xff5200,0xff5000,0xdeadbeef);
      CHECK(decoded.areg[0]==0xff5200 && decoded.areg[1]==0xff5000 && decoded.areg[2]==0xff5100 && decoded.dreg[0]==0xdeadbeef);
      // The VDP address is set by the caller, independently of D0.
      Set_VDP_Reg(15,2); Write_VDP_Ctrl(0x5800); Write_VDP_Ctrl(1);
      decoded=main_api(0x2ec,0xff5200,0xff5000,0x40000003);
      CHECK(decoded.areg[1]==0xff5000 && decoded.dreg[0]==0x40000003);
      for(unsigned i=0;i<32;i++)CHECK(VRam[0x5800+i]==Ram_68k[0x5100+i]);
      main68k_context.areg[2]=saved_a2;
      // DMA helpers restore the BIOS register shadow, not a temporary increment.
      Set_VDP_Reg(15,64);
      main_api(0x2d4,0,0,0x5a000001,0xff5100,2);
      CHECK(VDP_Reg.Auto_Inc==(rw(Ram_68k,0xfdd2)&255));
      CHECK(!Paused);
      for(unsigned i=0;i<32;i+=4) {
          unsigned v=((i/4)%2==0)?0xaaaa:0;
          CHECK(rw(Ram_68k,0x5100+i)==v && rw(Ram_68k,0x5102+i)==v);
      }
      CHECK(rw(Ram_68k,0x5120)==0xa5a5); }
    main_api(0x378,0,0,0x50,12); CHECK(rw(Ram_68k,0xfde0)==0x19 && rw(Ram_68k,0xfde2)==12);
    main_api(0x340); CHECK(rw(Ram_68k,0xfde0)==0 && M68K_RB(0xa1200e)==0);
    // Graphics/timer handlers patch the target only, retaining the BIOS JMP.
    for(unsigned level=1;level<=3;level+=2) {
        unsigned hook=level==1?0x5f76:0x5f82;
        CHECK(rw(Ram_Prg,hook)==0x4ef9);
        longword(Ram_Prg,hook+2,0x5500);
        word(Ram_Prg,0x5500,0x5279); longword(Ram_Prg,0x5502,0x5902); word(Ram_Prg,0x5506,0x4e73);
        S68000CONTEXT saved=sub68k_context;
        word(Ram_Prg,0x5400,0x60fe);
        sub68k_context.pc=0x5400; sub68k_context.areg[7]=0x5700; sub68k_context.sr=0x2000;
        sub68k_tripOdometer(); sub68k_interrupt(level,-1); CD_HLE_SubExec(500);
        CHECK(rw(Ram_Prg,0x5902)==(level==1?1:2));
        sub68k_context=saved;
    }
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
    // A Q-subcode probe before subcode collection has no buffered result.
    // Return carry, leave A0 and its buffer intact, and keep running.
    for(unsigned fn=0x93;fn<=0x94;fn++) {
        for(unsigned j=0;j<16;j+=2)word(Ram_Prg,0x5200+j,0xa55a);
        r=api(fn,0x5200,0x5300,0xfffe);
        CHECK((r.sr&1) && !Paused && r.areg[0]==0x5200 && r.areg[1]==0x5300);
        CHECK(r.dreg[0]==fn && r.dreg[1]==0xfffe);
        for(unsigned j=0;j<16;j+=2)CHECK(rw(Ram_Prg,0x5200+j)==0xa55a);
    }
    r=api(0x83,0,0,10); CHECK(r.dreg[0]==0x0004480a && !(r.dreg[1]&255));
    r=api(0x83,0,0,22); CHECK(r.dreg[0]==0x00046016 && !(r.dreg[1]&255));
    r=api(0x83,0,0,99); CHECK(r.dreg[0]==0x00020000);
    r=api(0x81); CHECK(r.areg[0]==0x1300 && rw(Ram_Prg,0x1310)==0x0116);
    // BIOS-only readers (without touching the host port) obey 75 sectors/s.
    longword(Ram_Prg,0x5200,33); longword(Ram_Prg,0x5204,3);
    api(0x20,0x5200);
    while(!(api(0x8a).sr&1))api(0x8d); // Drain earlier frame credits.
    for(int pal=0;pal<2;pal++) {
        CPU_Mode=pal;
        unsigned transferred=0;
        for(int tick=0;tick<(pal?50:60);tick++) {
            CD_HLE_Frame();
            // Starting another request must not replenish the drive budget.
            longword(Ram_Prg,0x5204,3); api(0x20,0x5200);
            unsigned burst=0;
            while(!(api(0x8a).sr&1)) {
                CHECK(!(api(0x8b).sr&1));
                CHECK(!(api(0x8c,0x20000,0x5300).sr&1));
                api(0x8d); transferred++; CHECK(++burst<=2);
            }
            CHECK(api(0x8b).sr&1);
            CHECK(api(0x8c,0x20000,0x5300).sr&1);
            api(0x8d); CHECK(api(0x8a).sr&1);
        }
        CHECK(transferred==75);
    }
    CPU_Mode=0; CD_HLE_Frame();
    // Adjacent Mode 1 reads advance by 2048 and never copy the EDC/ECC tail.
    longword(Ram_Prg,0x5200,33); longword(Ram_Prg,0x5204,1);
    r=api(0x20,0x5200); CHECK(!(r.sr&1));
    api(0x81);
    CHECK(rw(Ram_Prg,0x1308)==0x0002 && rw(Ram_Prg,0x130a)==0x3304);
    CHECK(rw(Ram_Prg,0x130c)==0 && rw(Ram_Prg,0x130e)==0x3304);
    memset(Ram_Prg+0x20000,0xa5,0x1000);
    r=api(0x8c,0x20000,0x5300);
    CHECK(!(r.sr&1) && r.areg[0]==0x20800 && r.areg[1]==0x5304);
    for(unsigned i=0;i<2048;i++)CHECK(Ram_Prg[(0x20000+i)^1]==data[33*2048+i]);
    CHECK(rw(Ram_Prg,0x20800)==0xa5a5);
    // The same API must write the initial sub-CPU 1M bank at $0C0000.
    r=api(0x8c,0xc0000,0x5300); CHECK(!(r.sr&1));
    for(unsigned i=0;i<2048;i++)CHECK(Ram_Word_1M[(0x20000+i)^1]==data[33*2048+i]);
    S68K_WB(0xff8004,3);
    r=api(0x8b); CHECK(!(r.sr&1) && (CDC.RS0&0xc000)==0x4000);
    CHECK(S68K_RW(0xff8008)==0x0002 && S68K_RW(0xff8008)==0x3301);
    for(unsigned i=0;i<2048;i+=2)CHECK(S68K_RW(0xff8008)==data[33*2048+i]*256+data[33*2048+i+1]);
    CHECK((CDC.RS0&0xc000)==0x8000 && !(SCD.Status_CDC&8));
    // CDC DMA copies only the payload, advances the destination and signals EDT.
    for(unsigned target=5;target<=7;target+=2) {
        S68K_WB(0xff8004,(unsigned char)target); CDC.DMA_Adr=0x4000;
        memset(Ram_Prg+0x20000,0xa5,0x810);
        memset(Ram_Word_1M+0x20000,0xa5,0x810);
        r=api(0x8b); CHECK(!(r.sr&1));
        for(unsigned i=0;i<8 && (SCD.Status_CDC&8);i++)Update_CDC_TRansfert();
        CHECK((CDC.RS0&0xc000)==0x8000 && !(SCD.Status_CDC&8));
        unsigned char *dest=target==5?Ram_Prg:Ram_Word_1M;
        for(unsigned i=0;i<2048;i++)CHECK(dest[(0x20000+i)^1]==data[33*2048+i]);
        CHECK(rw(dest,0x20800)==0xa5a5);
    }
    // Streaming pollers must wait for the next sector rather than outrun 75 Hz.
    longword(Ram_Prg,0x5204,3); api(0x20,0x5200);
    r=api(0x8a); CHECK(!(r.sr&1)); api(0x8d);
    r=api(0x8a); CHECK(r.sr&1);
    CD_HLE_Frame(); r=api(0x8a); CHECK(!(r.sr&1));
    // Pause is observable even before CDDA starts; seeks retain the track.
    api(0x03); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x500);
    word(Ram_Prg,0x5200,10); api(0x15,0x5200); api(0x81);
    CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x500 && Ram_Prg[0x1307^1]==10);
    CHECK(rw(Ram_Prg,0x1308)==0x0004 && rw(Ram_Prg,0x130a)==0x4800);
    CHECK(rw(Ram_Prg,0x130c)==0 && rw(Ram_Prg,0x130e)==0);
    api(0x04); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x100);
    word(Ram_68k,0xfdae,0); word(Ram_Prg,0x5e90,0);
    Reset_SegaCD();
    CHECK(CD_HLE_Active && rw(Ram_68k,0xfdae)==0x4ef9 && rw(Ram_Prg,0x5e90)==0x0116);
    Free_Rom(Game); FILE_End(); CHECK(!CD_HLE_Active);
    puts("PASS: HLE writable hooks, absent RAM cartridge, TOC/status times, 75 Hz BIOS/host transfers, Mode 1 transfers, reset and initial 1M banks");
    return 0;
}
