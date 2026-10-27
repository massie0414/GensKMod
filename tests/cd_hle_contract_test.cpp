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
#include "../src/Gens/program_ram_transfer.h"
#include "../src/Gens/main_word_ram_transfer.h"
#include "../src/Gens/vram_transfer.h"
#include "../src/Gens/genesis_ram_transfer.h"

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
static S68000CONTEXT bram_api(unsigned fn,unsigned a0=0,unsigned a1=0,unsigned d1=0) {
    S68000CONTEXT saved=sub68k_context;
    sub68k_context.pc=0x1216; sub68k_context.interrupts[0]=1;
    sub68k_context.areg[7]=0x5600; sub68k_context.areg[0]=a0; sub68k_context.areg[1]=a1;
    sub68k_context.dreg[0]=fn; sub68k_context.dreg[1]=d1;
    word(Ram_Prg,0x5600,0x2010); longword(Ram_Prg,0x5602,0x1400);
    CD_HLE_SubExec((int)sub68k_context.odometer);
    S68000CONTEXT result=sub68k_context; sub68k_context=saved; return result;
}
static S68000CONTEXT main_api(unsigned fn,unsigned a0=0,unsigned a1=0,unsigned d0=0,unsigned d1=0,unsigned d2=0,unsigned sr=0x2010) {
    S68000CONTEXT saved=main68k_context;
    main68k_context.pc=0x2006+(fn-0x200)*2; main68k_context.interrupts[0]=0x10;
    main68k_context.areg[7]=0xff5600; main68k_context.areg[0]=a0; main68k_context.areg[1]=a1;
    main68k_context.dreg[0]=d0; main68k_context.dreg[1]=d1; main68k_context.dreg[2]=d2;
    word(Ram_68k,0x5600,sr); longword(Ram_68k,0x5602,0xff5400);
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
      // HLE byte reads (Nemesis) and output writes also use the main-CPU counter.
      unsigned long main_values[MAIN_WORD_RAM_TRANSFER_HISTORY];
      for(unsigned i=0;i<sizeof(stream);i++)M68K_WB(0x200400+i,stream[i]);
      main68k_context.areg[2]=0x200500;
      MainWordRAMTransfer_Reset(); MainWordRAMTransfer_BeginFrame();
      main_api(0x2f0,0xff5200,0x200400,0xdeadbeef); MainWordRAMTransfer_EndFrame(60);
      CHECK(MainWordRAMTransfer_GetHistory(0,main_values)==1 && main_values[0]>2 && main_values[0]<=sizeof(stream));
      CHECK(MainWordRAMTransfer_GetHistory(1,main_values)==1 && main_values[0]==32);
      for(unsigned i=0;i<32;i++)CHECK(M68K_RB(0x200500+i)==Ram_68k[(0x5100+i)^1]);
      // HLE longword copies can read and write Word RAM in the same call.
      MainWordRAMTransfer_BeginFrame(); main_api(0x310,0x200500,0x200600,8); MainWordRAMTransfer_EndFrame(60);
      CHECK(MainWordRAMTransfer_GetHistory(0,main_values)==2 && main_values[1]==8);
      CHECK(MainWordRAMTransfer_GetHistory(1,main_values)==2 && main_values[1]==8);
      for(unsigned i=0;i<8;i++)CHECK(M68K_RB(0x200600+i)==M68K_RB(0x200500+i));
      main68k_context.areg[2]=0xff5100;
      // The VDP address is set by the caller, independently of D0.
      Set_VDP_Reg(15,2); Write_VDP_Ctrl(0x5800); Write_VDP_Ctrl(1);
      unsigned long vram_values[VRAM_TRANSFER_HISTORY];
      VRAMTransfer_Reset(); VRAMTransfer_BeginFrame();
      decoded=main_api(0x2ec,0xff5200,0xff5000,0x40000003);
      VRAMTransfer_EndFrame(60);
      CHECK(VRAMTransfer_GetHistory(0,vram_values)==1 && vram_values[0]==0);
      CHECK(VRAMTransfer_GetHistory(1,vram_values)==1 && vram_values[0]==32);
      CHECK(decoded.areg[1]==0xff5000 && decoded.dreg[0]==0x40000003);
      for(unsigned i=0;i<32;i++)CHECK(VRam[0x5800+i]==Ram_68k[0x5100+i]);
      main68k_context.areg[2]=saved_a2;
      // This HLE DMA helper uses CPU copies, so count its actual port writes.
      // It restores the BIOS register shadow, not a temporary increment.
      Set_VDP_Reg(15,64);
      VRAMTransfer_BeginFrame();
      main_api(0x2d4,0,0,0x5a000001,0xff5100,2);
      VRAMTransfer_EndFrame(60);
      CHECK(VRAMTransfer_GetHistory(0,vram_values)==2 && vram_values[1]==0);
      CHECK(VRAMTransfer_GetHistory(1,vram_values)==2 && vram_values[1]==4);
      CHECK(!memcmp(VRam+0x5a00,Ram_68k+0x5100,4));
      CHECK(VDP_Reg.Auto_Inc==(rw(Ram_68k,0xfdd2)&255));
      // HLE fill actually uses VDP DMA; its seed and payload are excluded.
      VRAMTransfer_BeginFrame();
      main_api(0x2c0,0,0,0x5c000001,3,0xab);
      VRAMTransfer_EndFrame(60);
      CHECK(VRAMTransfer_GetHistory(0,vram_values)==3 && vram_values[2]==0);
      CHECK(VRAMTransfer_GetHistory(1,vram_values)==3 && vram_values[2]==0);
      CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_DMA,0,vram_values)==3 && vram_values[2]==0);
      CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_DMA,1,vram_values)==3 && vram_values[0]==0 && vram_values[1]==0 && vram_values[2]==5);
      // HLE CPU fill writes are measured in the selected CPU channel.
      for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_VSRAM;kind++) {
          VRAMTransfer_BeginFrame();
          main_api(0x2b4,0,0,kind==VDP_TRANSFER_CRAM?0xc0000000:0x40000010,1,0x2468);
          VRAMTransfer_EndFrame(60);
          unsigned n=VDPTransfer_GetHistory(kind,1,vram_values);
          CHECK(n && vram_values[n-1]==4);
          CHECK(VDPTransfer_GetHistory(kind,0,vram_values)==n && vram_values[n-1]==0);
          CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_DMA,1,vram_values)==n && vram_values[n-1]==0);
      }
      CHECK(VRam[0x5c00]==0xab && VRam[0x5c02]==0xab);
      // Direct HLE palette and scroll-RAM stores retain their original VDP state.
      VRAMTransfer_BeginFrame();
      Ram_68k[0xfe29^1]|=1; word(Ram_68k,0xfb80,0x2468);
      unsigned address=Ctrl.Address,access=Ctrl.Access;
      main_api(0x2e8); main_api(0x2a8); VRAMTransfer_EndFrame(60);
      CHECK(Ctrl.Address==address && Ctrl.Access==access && rw(CRam,0)==0x2468 && rw(VSRam,0)==0);
      unsigned count=VDPTransfer_GetHistory(VDP_TRANSFER_CRAM,1,vram_values);
      CHECK(count && vram_values[count-1]==128);
      CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_VSRAM,1,vram_values)==count && vram_values[count-1]==80);
      VRAMTransfer_BeginFrame(); main_api(0x2e8); VRAMTransfer_EndFrame(60);
      count=VDPTransfer_GetHistory(VDP_TRANSFER_CRAM,1,vram_values);
      CHECK(vram_values[count-1]==0); // no pending palette upload
      VRAMTransfer_BeginFrame(); main_api(0x2a0); VRAMTransfer_EndFrame(60);
      count=VRAMTransfer_GetHistory(1,vram_values); CHECK(vram_values[count-1]==65536);
      CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_CRAM,1,vram_values)==count && vram_values[count-1]==2);
      CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_VSRAM,1,vram_values)==count && vram_values[count-1]==80);
      CHECK(VDPTransfer_GetHistory(VDP_TRANSFER_DMA,1,vram_values)==count && vram_values[count-1]==0);
      CHECK(!Paused);
      for(unsigned i=0;i<32;i+=4) {
          unsigned v=((i/4)%2==0)?0xaaaa:0;
          CHECK(rw(Ram_68k,0x5100+i)==v && rw(Ram_68k,0x5102+i)==v);
      }
      CHECK(rw(Ram_68k,0x5120)==0xa5a5); }
    // Main HLE data accesses include stack reads, direct RAM, and bus-mediated Z80 RAM.
    {
        unsigned long values[GENESIS_RAM_TRANSFER_HISTORY];
        GenesisRAMTransfer_Reset(); GenesisRAMTransfer_BeginFrame(); main_api(0x338,0,0,16); GenesisRAMTransfer_EndFrame(60);
        CHECK(GenesisRAMTransfer_GetHistory(0,0,values)==1 && values[0]==8);
        CHECK(GenesisRAMTransfer_GetHistory(0,1,values)==1 && values[0]==2);
        longword(Ram_68k,0x6000,0x12345678);longword(Ram_68k,0x6004,0x90abcdef);
        GenesisRAMTransfer_BeginFrame(); main_api(0x310,0xff6000,0xff6100,8); GenesisRAMTransfer_EndFrame(60);
        CHECK(GenesisRAMTransfer_GetHistory(0,0,values)==2 && values[1]==10);
        CHECK(GenesisRAMTransfer_GetHistory(0,1,values)==2 && values[1]==8);
        CHECK(!memcmp(Ram_68k+0x6000,Ram_68k+0x6100,8));
        int saved_z80=Z80_State; Z80_State=0;
        GenesisRAMTransfer_BeginFrame(); main_api(0x310,0xff6000,0xa00100,8); GenesisRAMTransfer_EndFrame(60);
        CHECK(GenesisRAMTransfer_GetHistory(0,0,values)==3 && values[2]==10);
        CHECK(GenesisRAMTransfer_GetHistory(1,1,values)==3 && values[2]==4);
        CHECK(M68K_RB(0xa00100)==0x34 && M68K_RB(0xa00102)==0x78);
        GenesisRAMTransfer_BeginFrame(); main_api(0x310,0xa00100,0xff6100,8); GenesisRAMTransfer_EndFrame(60);
        CHECK(GenesisRAMTransfer_GetHistory(0,0,values)==4 && values[3]==2);
        CHECK(GenesisRAMTransfer_GetHistory(0,1,values)==4 && values[3]==8);
        CHECK(GenesisRAMTransfer_GetHistory(1,0,values)==4 && values[3]==4);
        Z80_State=2;
        GenesisRAMTransfer_BeginFrame(); main_api(0x310,0xff6000,0xa00100,8); GenesisRAMTransfer_EndFrame(60);
        CHECK(GenesisRAMTransfer_GetHistory(1,1,values)==5 && values[4]==0); Z80_State=saved_z80;
        GenesisRAMTransfer_BeginFrame(); main_api(0x340); GenesisRAMTransfer_EndFrame(60);
        CHECK(GenesisRAMTransfer_GetHistory(0,0,values)==6 && values[5]==6);
        CHECK(GenesisRAMTransfer_GetHistory(0,1,values)==6 && values[5]==18);
    }
    main_api(0x378,0,0,0x50,12); CHECK(rw(Ram_68k,0xfde0)==0x19 && rw(Ram_68k,0xfde2)==12);
    // System commands occupy slot 1; ordinary CD commands in slot 0 survive.
    M68K_WB(0xa1200e,0x80);
    S68000CONTEXT queued=main_api(0x35c,0,0,0x12340004,0xabcd0004);
    CHECK(!Paused && rw(Ram_68k,0xfde4)==4 && rw(Ram_68k,0xfde6)==4);
    CHECK(rw(Ram_68k,0xfde0)==0x19 && M68K_RB(0xa1200e)==0x80);
    CHECK(queued.dreg[0]==0x12340004 && queued.dreg[1]==0xabcd0004);
    word(Ram_68k,0xfe2a,0);
    queued=main_api(0x338,0,0,16);
    CHECK(queued.dreg[0]==13 && queued.dreg[1]==0x5d350 && rw(Ram_68k,0xfe2a)==0x5d35 && !(queued.sr&31));
    word(Ram_68k,0xfe2a,1);
    queued=main_api(0x338,0,0,16);
    CHECK(queued.dreg[0]==1 && queued.dreg[1]==0xfff934e0 && (queued.sr&31)==16);
    word(Ram_68k,0xfe2a,0xffff);
    queued=main_api(0x33c);
    CHECK(queued.dreg[0]==0xffff271c && rw(Ram_68k,0xfe2a)==0x271c && (queued.sr&31)==17);
    main_api(0x350,0,0,0x12345678,0xdeadbeef);
    main_api(0x354,0,0,0x98765432,0xabcd1234);
    queued=main_api(0x358,0,0,0x5555abcd,0x777789ab);
    CHECK(rw(Ram_68k,0xfde0)==0x5678 && rw(Ram_68k,0xfde2)==0xbeef);
    CHECK(rw(Ram_68k,0xfde8)==0x5432 && rw(Ram_68k,0xfdea)==0x1234);
    CHECK(rw(Ram_68k,0xfdec)==0xabcd && rw(Ram_68k,0xfdee)==0x89ab);
    CHECK(queued.dreg[0]==0x5555abcd && queued.dreg[1]==0x777789ab && M68K_RB(0xa1200e)==0x80);
    main_api(0x35c,0,0,1,8); CHECK(M68K_RB(0xa1200e)==0x84);
    // Disc-status decoding honors the exchange and status-ready flags.
    Ram_68k[0xfdde^1]=2; Ram_68k[0xfddf^1]=4;
    word(Ram_68k,0xfdf4,0x101); word(Ram_68k,0xfdf6,0x010c);
    word(Ram_68k,0xfdfc,0x1234); word(Ram_68k,0xfdfe,0x5600);
    main_api(0x348); CHECK(rw(Ram_68k,0xfe3a)==0x101 && rw(Ram_68k,0xfe44)==0x010c);
    CHECK(rw(Ram_68k,0xfe40)==0x1234 && Ram_68k[0xfe42^1]==0x56);
    Ram_68k[0xfddf^1]=12;
    main_api(0x348); CHECK(rw(Ram_68k,0xfe3c)==0x010c && rw(Ram_68k,0xfe3e)==0x1234);
    CHECK(Ram_68k[0xfe43^1]==0x56 && (Ram_68k[0xfdde^1]&0x40));
    // DMA lists use explicit lengths and end at the first zero word.
    word(Ram_68k,0x5200,2); longword(Ram_68k,0x5202,0x5c000001); longword(Ram_68k,0x5206,0xff5100);
    word(Ram_68k,0x520a,1); longword(Ram_68k,0x520c,0x5c100001); longword(Ram_68k,0x5210,0xff5120);
    word(Ram_68k,0x5214,0);
    Set_VDP_Reg(15,64);
    queued=main_api(0x394,0,0xff5200);
    CHECK(!Paused && queued.areg[1]==0xff5216 && !(queued.dreg[2]&0xffff));
    CHECK(VDP_Reg.Auto_Inc==(rw(Ram_68k,0xfdd2)&255));
    for(unsigned j=0;j<4;j++)CHECK(VRam[0x5c00+j]==Ram_68k[0x5100+j]);
    CHECK(VRam[0x5c10]==Ram_68k[0x5120] && VRam[0x5c11]==Ram_68k[0x5121]);
    Set_VDP_Reg(15,64);
    queued=main_api(0x394,0,0xff5214); CHECK(queued.areg[1]==0xff5216 && !Paused && VDP_Reg.Auto_Inc==64);
    // The IP replacement helper tail-jumps to A2 and leaves the return on SP.
    { unsigned saved=main68k_context.areg[2]; main68k_context.areg[2]=0xff5300;
      memset(Ram_68k+0x5300,0x5a,12);
      queued=main_api(0x310,0xff5100,0xff5300,8);
      CHECK(!Paused && queued.pc==0xff5300 && queued.areg[7]==0xff5602);
      CHECK(queued.areg[0]==0xff5108 && queued.areg[1]==0xff5308);
      CHECK(!memcmp(Ram_68k+0x5300,Ram_68k+0x5100,8) && rw(Ram_68k,0x5308)==0x5a5a);
      main68k_context.areg[2]=saved; }
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
    api(0x81); CHECK(rw(Ram_Prg,0x1300)==0x0101);
    api(0x08); api(0x81); CHECK(rw(Ram_Prg,0x1300)==0x0505);
    api(0x09);
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
    // BRMDIR returns 16-byte names/offsets/block counts, supports prefix '*',
    // pagination, a full-buffer carry, and never writes beyond capacity.
    CHECK(!(bram_api(6).sr&1));
    Ram_Prg[0x5200^1]='*'; Ram_Prg[0x5201^1]=0;
    memset(Ram_Prg+0x5300,0xa5,48);
    unsigned long prg_values[PROGRAM_RAM_TRANSFER_HISTORY];
    ProgramRAMTransfer_Reset(); ProgramRAMTransfer_BeginFrame();
    CHECK(!(bram_api(7,0x5200,0x5300,2).sr&1));
    ProgramRAMTransfer_EndFrame(60);
    CHECK(ProgramRAMTransfer_GetHistory(0,prg_values)==1 && prg_values[0]==28);
    CHECK(ProgramRAMTransfer_GetHistory(1,prg_values)==1 && prg_values[0]==32);
    CHECK(rw(Ram_Prg,0x5300)==0 && rw(Ram_Prg,0x531e)==0 && rw(Ram_Prg,0x5320)==0xa5a5);
    for(unsigned file=0;file<2;file++) {
        const char *name=file?"SWITCH____2":"SWITCH____1";
        for(unsigned j=0;j<11;j++)Ram_Prg[(0x5200+j)^1]=name[j];
        Ram_Prg[0x520b^1]=file?255:0; word(Ram_Prg,0x520c,file+1);
        ProgramRAMTransfer_Reset(); ProgramRAMTransfer_BeginFrame();
        CHECK(!(bram_api(4,0x5200,0x5400).sr&1));
        ProgramRAMTransfer_EndFrame(60);
        CHECK(ProgramRAMTransfer_GetHistory(0,prg_values)==1 && prg_values[0]==96);
        CHECK(ProgramRAMTransfer_GetHistory(1,prg_values)==1 && prg_values[0]==0);
        ProgramRAMTransfer_BeginFrame(); CHECK(!(bram_api(3,0x5200,0x5500).sr&1)); ProgramRAMTransfer_EndFrame(60);
        CHECK(ProgramRAMTransfer_GetHistory(0,prg_values)==2 && prg_values[1]==28);
        CHECK(ProgramRAMTransfer_GetHistory(1,prg_values)==2 && prg_values[1]==64);
        ProgramRAMTransfer_BeginFrame(); CHECK(!(bram_api(8,0x5200,0x5500).sr&1)); ProgramRAMTransfer_EndFrame(60);
        CHECK(ProgramRAMTransfer_GetHistory(0,prg_values)==3 && prg_values[2]==92);
        CHECK(ProgramRAMTransfer_GetHistory(1,prg_values)==3 && prg_values[2]==0);
    }
    Ram_Prg[0x520a^1]='*'; Ram_Prg[0x520b^1]=0;
    CHECK(bram_api(7,0x5200,0x5300,1).sr&1);
    CHECK(Ram_Prg[0x530a^1]=='1' && Ram_Prg[0x530b^1]==0 && rw(Ram_Prg,0x530e)==1);
    CHECK(!(bram_api(7,0x5200,0x5300,0x10001).sr&1));
    CHECK(Ram_Prg[0x530a^1]=='2' && rw(Ram_Prg,0x530e)==2);
    Ram_Prg[0x5200^1]='X';
    CHECK(!(bram_api(7,0x5200,0x5300,1).sr&1) && rw(Ram_Prg,0x5300)==0);
    CHECK(bram_api(7,0x5200,0x7fff0,2).sr&1);
    CHECK(!(bram_api(6).sr&1));
    // Pause is observable even before CDDA starts; seeks retain the track.
    api(0x03); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x500);
    word(Ram_Prg,0x5200,10); api(0x15,0x5200); api(0x81);
    CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x500 && Ram_Prg[0x1307^1]==10);
    CHECK(rw(Ram_Prg,0x1308)==0x0004 && rw(Ram_Prg,0x130a)==0x4800);
    CHECK(rw(Ram_Prg,0x130c)==0 && rw(Ram_Prg,0x130e)==0);
    api(0x04); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x100);
    api(0x19,0x5200); api(0x81);
    CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x800 && !CD_HLE_AudioPlaying());
    CD_HLE_Frame(); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x800);
    CD_HLE_Frame(); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x800 && !CD_HLE_AudioPlaying());
    for(unsigned i=2;i<15;i++)CD_HLE_Frame();
    api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x800);
    CD_HLE_Frame(); api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x500 && !CD_HLE_AudioPlaying());
    for(unsigned i=0;i<120;i++)CD_HLE_Frame();
    api(0x81); CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x500 && rw(Ram_Prg,0x130c)==0 && rw(Ram_Prg,0x130e)==0);
    api(0x04); CD_HLE_Frame(); api(0x81);
    CHECK((rw(Ram_Prg,0x1300)&0xff00)==0x100 && CD_HLE_AudioPlaying() && rw(Ram_Prg,0x130e)!=0);
    word(Ram_68k,0xfdae,0); word(Ram_Prg,0x5e90,0);
    Reset_SegaCD();
    CHECK(CD_HLE_Active && rw(Ram_68k,0xfdae)==0x4ef9 && rw(Ram_Prg,0x5e90)==0x0116);
    // SET_VINT clients that send their own IRQ2 must not get a second
    // heartbeat. A stale extra interrupt can abort the next CD read.
    word(Ram_Prg,0x5900,0);
    word(Ram_Prg,0x5500,0x5279); longword(Ram_Prg,0x5502,0x5900); word(Ram_Prg,0x5506,0x4e75);
    longword(Ram_Prg,0x5f36,0x5500);
    word(Ram_Prg,0x5400,0x60fe);
    sub68k_context.pc=0x5400; sub68k_context.areg[7]=0x5700; sub68k_context.sr=0x2000;
    sub68k_tripOdometer(); sub68k_context.interrupts[0]=0;
    CD_HLE_Frame(); CD_HLE_SubExec(500); CHECK(rw(Ram_Prg,0x5900)==1);
    main_api(0x368,0,0xff5400);
    main_api(0x360,0,0,0,0,0,0x2600);
    CD_HLE_SubExec(1000); CHECK(rw(Ram_Prg,0x5900)==2);
    CD_HLE_Frame(); CD_HLE_SubExec(1500); CHECK(rw(Ram_Prg,0x5900)==2);

    Free_Rom(Game); FILE_End(); CHECK(!CD_HLE_Active);
    puts("PASS: HLE writable hooks, absent RAM cartridge, TOC/status times, 75 Hz BIOS/host transfers, Mode 1 transfers, reset and initial 1M banks");
    return 0;
}
