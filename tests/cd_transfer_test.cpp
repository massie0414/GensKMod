// Synthetic data and window tests; no game or BIOS files required.
#include <windows.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include "../src/Gens/gens.h"
#include "../src/Gens/kmod/common.h"
}
#include "../src/Gens/G_main.h"
#include "../src/Gens/rom.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Cpu_SH2.h"
#include "../src/Gens/Mem_SH2.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/Mem_S68K.h"
#include "../src/Gens/cd_file.h"
#include "../src/Gens/cd_hle.h"
#include "../src/Gens/cd_sys.h"
#include "../src/Gens/cd_aspi.h"
#include "../src/Gens/cd_transfer.h"
#include "../src/Gens/word_ram_transfer.h"
#include "../src/Gens/main_word_ram_transfer.h"
#include "../src/Gens/vram_transfer.h"
#include "../src/Gens/genesis_ram_transfer.h"
#include "../src/Gens/sh2_dma_transfer.h"
#include "../src/Gens/pwm.h"
#include "../src/Gens/program_ram_transfer.h"
#include "../src/Gens/LC89510.h"
#include "../src/Gens/G_dsound.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/cd_transfer_panel.h"
extern int Sectors_In_Cache; // ASPI cache state, supplied synthetically below.

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static unsigned long values[CD_TRANSFER_HISTORY];
static void sample(unsigned long bytes, unsigned fps = 60) {
    CDTransfer_BeginFrame(); CDTransfer_AddBytes(bytes); CDTransfer_EndFrame(fps);
}
static unsigned long latest(void) {
    unsigned count = CDTransfer_GetHistory(values);
    return count ? values[count - 1] : ~0UL;
}
static void word(unsigned char *p, unsigned a, unsigned v) { p[a^1]=v>>8; p[(a+1)^1]=v; }
static void longword(unsigned char *p, unsigned a, unsigned v) { word(p,a,v>>16); word(p,a+2,v); }
static void be32(unsigned char *p, unsigned v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static unsigned api(unsigned fn, unsigned a0 = 0, unsigned a1 = 0) {
    S68000CONTEXT saved = sub68k_context;
    sub68k_context.pc=0x1206; sub68k_context.interrupts[0]=1;
    sub68k_context.areg[7]=0x5600; sub68k_context.areg[0]=a0; sub68k_context.areg[1]=a1;
    sub68k_context.dreg[0]=fn;
    word(Ram_Prg,0x5600,0x2010); longword(Ram_Prg,0x5602,0x1400);
    CD_HLE_SubExec((int)sub68k_context.odometer);
    unsigned result=sub68k_context.sr; sub68k_context=saved; return result;
}
static void request(unsigned lba, unsigned count) {
    longword(Ram_Prg,0x5200,lba); longword(Ram_Prg,0x5204,count); api(0x20,0x5200);
}

static int history_tests(void) {
    CDTransfer_Reset(); CHECK(!CDTransfer_GetHistory(values));
    CDTransfer_AddBytes(2048); CDTransfer_EndFrame(60); CHECK(!CDTransfer_GetHistory(values));
    for (unsigned i=0;i<300;i++) sample(i);
    CHECK(CDTransfer_GetHistory(values)==256 && values[0]==44 && values[255]==299);
    CDTransfer_Reset();
    for (unsigned fps=50;fps<=60;fps+=10) {
        unsigned clock=0;
        CDTransfer_Reset();
        for (unsigned i=0;i<fps;i++) {
            clock+=75; sample((clock/fps)*2048,fps); clock%=fps;
        }
        CHECK(CDTransfer_GetBytesPerSecond()==150*1024);
        for (unsigned i=0;i<fps;i++) sample(0,fps);
        CHECK(CDTransfer_GetBytesPerSecond()==0);
    }
    sample(2048,50); CHECK(CDTransfer_GetHistory(values)==1 && values[0]==2048);
    CDTransfer_EndFrame(50); CHECK(CDTransfer_GetHistory(values)==1);
    puts("PASS: 256-frame rollover, PAL/NTSC 150 KiB/s, idle decay, reset and no duplicate samples");
    return 0;
}

static int drive_tests(void) {
    unsigned char sector[2352]; memset(sector,0x5a,sizeof(sector));
    for (unsigned format=TYPE_ISO;format<=TYPE_BIN;format++) {
        Tracks[0].F=tmpfile(); CHECK(Tracks[0].F);
        CHECK(fwrite(sector,1,format==TYPE_ISO?2048:2352,Tracks[0].F)>0);
        fflush(Tracks[0].F); Tracks[0].Type=format; Tracks[0].Lenght=1;
        CDD.Control=0x100; CDC.CTRL.B.B0=0x84; CDC.IFCTRL=0; SCD.Cur_LBA=0;
        CDTransfer_Reset(); CDTransfer_BeginFrame();
        CHECK(FILE_Read_One_LBA_CDC()==0); CDTransfer_EndFrame(60);
        CHECK(latest()==2048);
        for (unsigned ctrl=0;ctrl<=0x80;ctrl+=0x80) {
            CDC.CTRL.B.B0=ctrl; SCD.Cur_LBA=0; CDTransfer_BeginFrame();
            CHECK(FILE_Read_One_LBA_CDC()==0); CDTransfer_EndFrame(60); CHECK(latest()==0);
        }
        // A nominal second sector past the file's EOF must not be counted.
        CDC.CTRL.B.B0=0x84; SCD.Cur_LBA=1; Tracks[0].Lenght=2;
        CDTransfer_BeginFrame(); FILE_Read_One_LBA_CDC(); CDTransfer_EndFrame(60); CHECK(latest()==0);
        // CD-DA uses the same reader but is excluded from data throughput.
        CDD.Control=0; SCD.TOC.First_Track=SCD.Cur_Track=1;
        Tracks[0].Type=TYPE_CDDA; SCD.Cur_LBA=0;
        CDTransfer_BeginFrame(); FILE_Read_One_LBA_CDC(); CDTransfer_EndFrame(60); CHECK(latest()==0);
        Unload_ISO(); CHECK(!CDTransfer_GetHistory(values));
    }
    // Physical-drive cache delivery, without invoking ASPI or a real drive.
    CDD.Control=0x100; CDC.CTRL.B.B0=0x84; CDC.IFCTRL=0; SCD.Status_CDC=1;
    Sectors_In_Cache=1; SCD.Cur_LBA=0;
    CDTransfer_BeginFrame(); ASPI_Read_One_CDC_Cache(); CDTransfer_EndFrame(60);
    CHECK(latest()==2048 && Sectors_In_Cache==0);
    CDTransfer_BeginFrame(); CHECK(ASPI_Read_One_CDC_Cache()==-1); CDTransfer_EndFrame(60); CHECK(latest()==0);
    LC89510_Reset(); CHECK(!CDTransfer_GetHistory(values));
    puts("PASS: ISO/BIN and physical cache delivery, disabled decoding, EOF, CD-DA exclusion");
    return 0;
}

static int hle_tests(void) {
    static unsigned char data[64*2048];
    memcpy(data,"SEGADISCSYSTEM",14);
    be32(data+0x30,0x800); be32(data+0x34,0x40); be32(data+0x40,0x800); be32(data+0x44,0x40);
    data[0x200]=0x60; data[0x201]=0xfe; be32(data+0x818,0x20);
    data[0x821]=0x10; data[0x823]=0x12; data[0x825]=0x14; data[0x827]=0x16;
    data[0x830]=0x4e; data[0x831]=0x75; data[0x832]=0x60; data[0x833]=0xfe;
    data[0x834]=data[0x836]=0x4e; data[0x835]=data[0x837]=0x75;
    FILE *f=fopen("transfer-test.iso","wb"); CHECK(f);
    CHECK(fwrite(data,1,sizeof(data),f)==sizeof(data)); fclose(f);
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path); strcat(Gens_Path,"\\");
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0; Country=0; Sound_Enable=0; Sound_Rate=44100;
    MSH2_Init(); SSH2_Init(); M68K_Init(); S68K_Init(); Z80_Init(); Init_Tab();
    SegaCD_Started=Init_SegaCD("transfer-test.iso"); CHECK(SegaCD_Started && CD_HLE_Active);
    CHECK(!CDTransfer_GetHistory(values)); // Boot metadata is excluded.
    CDTransfer_BeginFrame(); CD_HLE_Frame(); request(33,2); CDC.RS0=0x300;
    CHECK(!(api(0x8b)&1)); CHECK(!(api(0x8b)&1));
    CHECK(!(api(0x8c,0x6000,0x5100)&1)); // Same pending sector through both APIs.
    CDTransfer_EndFrame(60); CHECK(latest()==2048);
    CDTransfer_BeginFrame(); CHECK(!(api(0x8c,0x6000,0x5100)&1));
    api(0x8d); CHECK(!(api(0x8c,0x6000,0x5100)&1)); api(0x8d);
    CDTransfer_EndFrame(60); CHECK(latest()==2048);
    CDTransfer_BeginFrame(); CHECK(api(0x8c,0x6000,0x5100)&1); api(0x8d);
    CDTransfer_EndFrame(60); CHECK(latest()==0);
    // Explicit reread is a new transfer; status/acknowledge without data is not.
    CDTransfer_BeginFrame(); CD_HLE_Frame(); request(33,1); CDC.RS0=0;
    CHECK(!(api(0x8b)&1)); CHECK(!(api(0x8c,0x6000,0x5100)&1)); api(0x8d);
    CDTransfer_EndFrame(60); CHECK(latest()==2048);
    CDTransfer_BeginFrame(); CD_HLE_Frame(); request(33,1); api(0x8d);
    CDTransfer_EndFrame(60); CHECK(latest()==0);
    // BIOS HLE's sub-CPU copies use the same measured data-access path.
    unsigned long ram_values[WORD_RAM_TRANSFER_HISTORY];
    Ram_Word_State=1; MS68K_Set_Word_Ram();
    S68K_WW(0x080100,0); S68K_WW(0x080102,33);
    S68K_WW(0x080104,0); S68K_WW(0x080106,1);
    WordRAMTransfer_Reset(); WordRAMTransfer_BeginFrame(); CD_HLE_Frame();
    CHECK(!(api(0x20,0x080100)&1));
    CHECK(!(api(0x8c,0x082000,0x083000)&1)); api(0x8d);
    WordRAMTransfer_EndFrame(60);
    CHECK(WordRAMTransfer_GetHistory(0,ram_values)==1 && ram_values[0]==8);
    CHECK(WordRAMTransfer_GetHistory(1,ram_values)==1 && ram_values[0]==2052);
    WordRAMTransfer_BeginFrame(); CD_HLE_Frame(); request(33,1); CDC.RS0=0x700;
    CHECK(!(api(0x8b)&1)); CDC.DMA_Adr=0;
    while(SCD.Status_CDC&8) Update_CDC_TRansfert();
    api(0x8d); WordRAMTransfer_EndFrame(60);
    CHECK(WordRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[1]==0);
    CHECK(WordRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[1]==0);
    // HLE parameter/stack reads and payload/header writes in Program RAM.
    ProgramRAMTransfer_Reset(); ProgramRAMTransfer_BeginFrame(); CD_HLE_Frame(); request(33,1);
    CHECK(!(api(0x8c,0x20000,0x5100)&1)); api(0x8d); ProgramRAMTransfer_EndFrame(60);
    CHECK(ProgramRAMTransfer_GetHistory(0,ram_values)==1 && ram_values[0]==26);
    CHECK(ProgramRAMTransfer_GetHistory(1,ram_values)==1 && ram_values[0]==2052);
    ProgramRAMTransfer_BeginFrame(); CHECK(!(api(0x81)&1)); ProgramRAMTransfer_EndFrame(60);
    CHECK(ProgramRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[1]==6);
    CHECK(ProgramRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[1]==45);
    ProgramRAMTransfer_BeginFrame(); CD_HLE_Frame(); request(33,1); CDC.RS0=0x500;
    CHECK(!(api(0x8b)&1)); CDC.DMA_Adr=0x4000;
    while(SCD.Status_CDC&8) Update_CDC_TRansfert();
    api(0x8d); ProgramRAMTransfer_EndFrame(60);
    CHECK(ProgramRAMTransfer_GetHistory(0,ram_values)==3 && ram_values[2]==26);
    CHECK(ProgramRAMTransfer_GetHistory(1,ram_values)==3 && ram_values[2]==0);
    Reset_SegaCD(); CHECK(!CDTransfer_GetHistory(values));
    CHECK(!ProgramRAMTransfer_GetHistory(0,ram_values));
    CHECK(!WordRAMTransfer_GetHistory(0,ram_values));
    CHECK(!MainWordRAMTransfer_GetHistory(0,ram_values));
    CHECK(!VRAMTransfer_GetHistory(0,ram_values));
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(!GenesisRAMTransfer_GetHistory(kind,0,ram_values));
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(!VDPTransfer_GetHistory(kind,0,ram_values));
    // Both CPUs exercise independent sampling in every real frame path.
    // Native loop exercises each CPU VDP target and starts a real memory DMA.
    unsigned cursor=0x5400;
    word(Ram_68k,cursor,0x3010); cursor+=2; word(Ram_68k,cursor,0x3280); cursor+=2;
    word(Ram_68k,cursor,0x3014); cursor+=2; word(Ram_68k,cursor,0x3a80); cursor+=2;
    word(Ram_68k,cursor,0x1016); cursor+=2; word(Ram_68k,cursor,0x1c80); cursor+=2;
    main68k_context.areg[4]=0xff1000; main68k_context.areg[5]=0xff1100;
    main68k_context.areg[6]=0xa00100; Z80_State=0;
    const unsigned write_commands[]={0x40000000,0xc0000000,0x40000010};
    const unsigned read_commands[]={0,0x20,0x10};
    for(unsigned k=0;k<3;k++) {
        word(Ram_68k,cursor,0x26bc); longword(Ram_68k,cursor+2,write_commands[k]); cursor+=6;
        word(Ram_68k,cursor,0x3480); cursor+=2;
        word(Ram_68k,cursor,0x26bc); longword(Ram_68k,cursor+2,read_commands[k]); cursor+=6;
        word(Ram_68k,cursor,0x3012); cursor+=2;
    }
    const unsigned dma_regs[]={0x8110,0x8f02,0x9302,0x9400,0x9500,0x9688,0x977f};
    for(unsigned k=0;k<sizeof(dma_regs)/sizeof(dma_regs[0]);k++) {
        word(Ram_68k,cursor,0x36bc); word(Ram_68k,cursor+2,dma_regs[k]); cursor+=4;
    }
    word(Ram_68k,cursor,0x26bc); longword(Ram_68k,cursor+2,0x40000080); cursor+=6;
    word(Ram_68k,cursor,0x6000|((0x5400-cursor-2)&255));
    main68k_context.areg[2]=0xc00000; main68k_context.areg[3]=0xc00004;
    main68k_context.pc=0xff5400; main68k_context.sr=0x2700;
    main68k_context.areg[0]=0x200100; main68k_context.areg[1]=0x200200;
    word(Ram_Prg,0x5400,0x3010); word(Ram_Prg,0x5402,0x3280); word(Ram_Prg,0x5404,0x3012);
    word(Ram_Prg,0x5406,0x3680); word(Ram_Prg,0x5408,0x60f6);
    sub68k_context.pc=0x5400; sub68k_context.sr=0x2700; sub68k_context.interrupts[0]=0;
    sub68k_context.areg[0]=0x080100; sub68k_context.areg[1]=0x080200;
    sub68k_context.areg[2]=0x060100; sub68k_context.areg[3]=0x060200;
    Ram_Word_State=1; MS68K_Set_Word_Ram(); S68K_State=1;
    int (*frames[])(void)={Do_SegaCD_Frame,Do_SegaCD_Frame_No_VDP,
        Do_SegaCD_Frame_Cycle_Accurate,Do_SegaCD_Frame_No_VDP_Cycle_Accurate};
    CD_HLE_Active=0; SCD.Status_CDC=0;
    for (unsigned i=0;i<4;i++) {
        CDTransfer_Reset(); VRAMTransfer_Reset(); GenesisRAMTransfer_Reset(); WordRAMTransfer_Reset(); MainWordRAMTransfer_Reset(); ProgramRAMTransfer_Reset(); frames[i](); CHECK(CDTransfer_GetHistory(values)==1);
        CHECK(WordRAMTransfer_GetHistory(0,ram_values)==1 && ram_values[0]>0);
        CHECK(MainWordRAMTransfer_GetHistory(0,ram_values)==1 && ram_values[0]>0);
        CHECK(VRAMTransfer_GetHistory(0,ram_values)==1 && ram_values[0]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,0,ram_values)==1 && ram_values[0]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,0,ram_values)==1 && ram_values[0]>0);
        CHECK(ProgramRAMTransfer_GetHistory(0,ram_values)==1 && ram_values[0]>0);
        CHECK(WordRAMTransfer_GetHistory(1,ram_values)==1 && ram_values[0]>0);
        CHECK(MainWordRAMTransfer_GetHistory(1,ram_values)==1 && ram_values[0]>0);
        CHECK(VRAMTransfer_GetHistory(1,ram_values)==1 && ram_values[0]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,1,ram_values)==1 && ram_values[0]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,1,ram_values)==1 && ram_values[0]>0);
        CHECK(ProgramRAMTransfer_GetHistory(1,ram_values)==1 && ram_values[0]>0);
    }
    CD_HLE_Active=1;
    CDTransfer_Reset(); VRAMTransfer_Reset(); GenesisRAMTransfer_Reset(); WordRAMTransfer_Reset(); MainWordRAMTransfer_Reset(); ProgramRAMTransfer_Reset(); Do_SegaCD_Frame(); Do_SegaCD_Frame_No_VDP(); CHECK(CDTransfer_GetHistory(values)==2);
    CHECK(WordRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(MainWordRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(VRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(ProgramRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(WordRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(MainWordRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(VRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(ProgramRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    Enable_CD_32X(); Z80_State=0;
    // Real PWM requests drive both SH2 DMACs throughout rendered/skipped frames.
    PWM_Init(); PWM_Enable=1; _32X_MINT=_32X_SINT=0;
    for(int owner=0;owner<2;owner++) {
        SH2_CONTEXT *cpu=owner?&S_SH2:&M_SH2;
        SH2_DMA1_Request(cpu,0);
        SH2_Write_Long(cpu,0xffffff9c,0); SH2_Write_Long(cpu,0xffffffb0,1);
        SH2_Write_Long(cpu,0xffffff90,0x06001000); SH2_Write_Long(cpu,0xffffff94,0x20004034);
        SH2_Write_Long(cpu,0xffffff98,100000); SH2_Write_Long(cpu,0xffffff9c,0x08e1);
    }
    PWM_Mode=0x185; PWM_Set_Cycle(32); PWM_Set_Int(1); SH2DMATransfer_Reset();
    CDTransfer_Reset(); VRAMTransfer_Reset(); GenesisRAMTransfer_Reset(); WordRAMTransfer_Reset(); MainWordRAMTransfer_Reset(); ProgramRAMTransfer_Reset(); Do_32X_Frame(); Do_32X_Frame_No_VDP(); CHECK(CDTransfer_GetHistory(values)==2);
    CHECK(WordRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(MainWordRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(VRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(ProgramRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(WordRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(MainWordRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(VRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(ProgramRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int owner=0;owner<2;owner++) for(int write=0;write<2;write++)
        CHECK(SH2DMATransfer_GetHistory(owner,write,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    // The same Genesis VDP is sampled without the CD add-on as well.
    CD_32X_Active=SegaCD_Started=0; CD_HLE_Close(); _32X_Started=1; Init_Memory_M68K(1);
    SH2DMATransfer_Reset();
    VRAMTransfer_Reset(); GenesisRAMTransfer_Reset(); Do_32X_Frame(); Do_32X_Frame_No_VDP();
    for(int owner=0;owner<2;owner++) for(int write=0;write<2;write++)
        CHECK(SH2DMATransfer_GetHistory(owner,write,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);

    CHECK(VRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(VRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    PWM_Enable=0; SH2DMATransfer_Reset();
    _32X_Started=0; Genesis_Started=1; Init_Memory_M68K(0);
    VRAMTransfer_Reset(); GenesisRAMTransfer_Reset(); Do_Genesis_Frame(); Do_Genesis_Frame_No_VDP();
    CHECK(VRAMTransfer_GetHistory(0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,0,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(VRAMTransfer_GetHistory(1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=GENESIS_RAM_MAIN;kind<=GENESIS_RAM_SUB;kind++) CHECK(GenesisRAMTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_DMA;kind++) CHECK(VDPTransfer_GetHistory(kind,1,ram_values)==2 && ram_values[0]>0 && ram_values[1]>0);
    CHECK(!SH2DMATransfer_GetHistory(0,0,ram_values));
    Genesis_Started=0; Unload_ISO(); DeleteFile("transfer-test.iso");
    puts("PASS: HLE direct/host reads without double counting; all eight frame loops including Genesis, SegaCD, 32X/CD32X and frameskip");
    return 0;
}

static int panel_tests(void) {
    char config[MAX_PATH], folder[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"cdt",0,config));
    SegaCD_Started=1; CD_32X_Active=0; Paused=0;
    cdtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_CD_TRANSFER-1]; CHECK(panel);
    cdtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    CDTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(2048);
    cdtransfer_update(); GetDlgItemText(panel,IDC_CD_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HWND graph=GetDlgItem(panel,IDC_CD_TRANSFER_GRAPH);
    HDC screen=GetDC(graph), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hwndItem=graph; item.hDC=dc;
    SetRect(&item.rcItem,0,0,500,180);
    SendMessage(panel,WM_DRAWITEM,IDC_CD_TRANSFER_GRAPH,(LPARAM)&item);
    CHECK(GetPixel(dc,100,40)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,140)==RGB(0,0,255));
    Paused=1;
    for (unsigned i=0;i<10;i++) { cdtransfer_update(); SendMessage(panel,WM_DRAWITEM,IDC_CD_TRANSFER_GRAPH,(LPARAM)&item); }
    CHECK(CDTransfer_GetHistory(values)==256 && values[255]==2048);
    GetDlgItemText(panel,IDC_CD_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"Paused"));
    SegaCD_Started=0; cdtransfer_update(); SendMessage(panel,WM_DRAWITEM,IDC_CD_TRANSFER_GRAPH,(LPARAM)&item);
    CHECK(GetPixel(dc,100,140)==GetSysColor(COLOR_WINDOW));
    GetDlgItemText(panel,IDC_CD_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"inactive"));
    CD_32X_Active=1; cdtransfer_update(); GetDlgItemText(panel,IDC_CD_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0"));
    CD_32X_Active=0;
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(graph,screen);
    // Ensure the text controls fit their font and the dialog's client area.
    RECT client; GetClientRect(panel,&client); MapWindowPoints(panel,NULL,(POINT*)&client,2);
    for (HWND control=GetWindow(panel,GW_CHILD);control;control=GetWindow(control,GW_HWNDNEXT)) {
        RECT bounds, required={0}; GetWindowRect(control,&bounds);
        CHECK(bounds.left>=client.left && bounds.right<=client.right && bounds.bottom<=client.bottom);
        if (control==graph) continue;
        GetWindowText(control,text,sizeof(text));
        HDC dc=GetDC(control); HGDIOBJ font=SelectObject(dc,(HFONT)SendMessage(control,WM_GETFONT,0,0));
        DrawText(dc,text,-1,&required,DT_CALCRECT|DT_LEFT|DT_NOPREFIX);
        SelectObject(dc,font); ReleaseDC(control,dc);
        CHECK(required.right<=bounds.right-bounds.left && required.bottom<=bounds.bottom-bounds.top);
    }
    OpenWindow_KMod(DMODE_CD_TRANSFER); GetWindowRect(panel,&saved); cdtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !CDTransfer_GetHistory(values));
    cdtransfer_destroy(); CHECK(!HandleWindow_KMod[DMODE_CD_TRANSFER-1]);
    cdtransfer_create(GetModuleHandle(NULL),NULL); cdtransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_CD_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    cdtransfer_save_window(config); cdtransfer_destroy();
    cdtransfer_create(GetModuleHandle(NULL),NULL); cdtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_CD_TRANSFER-1]); cdtransfer_destroy(); DeleteFile(config);
    puts("PASS: blue graph, inactive/CD32X/paused display, layout, position/visibility persistence and ROM close");
    return 0;
}

int main(void) {
    if (history_tests() || drive_tests() || hle_tests() || panel_tests()) return 1;
    return 0;
}
