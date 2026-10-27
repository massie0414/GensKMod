// Native main-68000 instructions and real dialog resources; no game/BIOS needed.
#include <windows.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include "../src/Gens/gens.h"
#include "../src/Gens/kmod/common.h"
}
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/Mem_S68K.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/cd_sys.h"
#include "../src/Gens/LC89510.h"
#include "../src/Gens/vram_transfer.h"
#include "../src/Gens/genesis_ram_transfer.h"
#include "../src/Gens/main_word_ram_transfer.h"
#include "../src/Gens/Mem_Z80.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/genesis_ram_transfer_panel.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static void opcode(unsigned address, unsigned value) { M68K_WW(address,(unsigned short)value); }
static void run(unsigned pc=0xff0200, unsigned cycles=1) {
    main68k_context.pc=pc; main68k_context.sr=0x2700;
    memset(main68k_context.interrupts,0,sizeof(main68k_context.interrupts));
    main68k_tripOdometer(); main68k_exec(cycles);
}

static unsigned long values[GENESIS_RAM_TRANSFER_HISTORY];
static unsigned long latest(int kind,int write) {
    unsigned n=GenesisRAMTransfer_GetHistory(kind,write,values);return n?values[n-1]:~0UL;
}
static void sample(unsigned reads,unsigned writes,unsigned fps=60) {
    Z80_State=0; GenesisRAMTransfer_BeginFrame();
    for(int kind=0;kind<2;kind++) {
        unsigned base=kind?0xa00100:0xff1000;
        for(unsigned i=0;i<reads;i++)M68K_Data_RB(base);
        for(unsigned i=0;i<writes;i++)M68K_Data_WB(base,0);
    }
    GenesisRAMTransfer_EndFrame(fps);
}
static int instruction_tests(void) {
    const unsigned readops[]={0x1010,0x3010,0x2010},writeops[]={0x1280,0x3280,0x2280};
    Rom_Size=0x20000;
    for(unsigned system=0;system<3;system++) {
        M68K_Reset(system); Reset_VDP();
        Genesis_Started=system==0; _32X_Started=system==1; SegaCD_Started=system==2; CD_32X_Active=0; Z80_State=0;
        for(int kind=0;kind<2;kind++) {
            const unsigned main_bases[]={0xe00000,0xe10000,0xf00000,0xff0000};
            const unsigned sub_bases[]={0xa00000,0xa02000,0xa08000,0xa0a000};
            for(unsigned mirror=0;mirror<4;mirror++) {
                unsigned base=kind?sub_bases[mirror]:main_bases[mirror];
                for(unsigned size=0;size<3;size++) {
                    opcode(base+0x1000,0x1234); opcode(base+0x1002,0x5678);
                    unsigned expected=size==0?M68K_RB(base+0x1000):size==1?M68K_RW(base+0x1000):((unsigned)M68K_RW(base+0x1000)<<16)|M68K_RW(base+0x1002);
                    unsigned bytes=kind?(size==2?2:1):(1<<size);
                    opcode(0xff0200,readops[size]); opcode(0xff0202,writeops[size]);
                    main68k_context.areg[0]=base+0x1000; main68k_context.areg[1]=base+0x1100;
                    main68k_context.dreg[0]=0;
                    GenesisRAMTransfer_Reset(); GenesisRAMTransfer_BeginFrame(); run();
                    CHECK(main68k_context.pc==0xff0202 && main68k_context.dreg[0]==expected);
                    run(0xff0202); GenesisRAMTransfer_EndFrame(60);
                    if(latest(kind,0)!=bytes || latest(kind,1)!=bytes || latest(1-kind,0) || latest(1-kind,1)) printf("system=%u kind=%d mirror=%u size=%u count=%lu/%lu other=%lu/%lu\n",system,kind,mirror,size,latest(kind,0),latest(kind,1),latest(1-kind,0),latest(1-kind,1));
                    CHECK(latest(kind,0)==bytes && latest(kind,1)==bytes && latest(1-kind,0)==0 && latest(1-kind,1)==0);
                    CHECK(size==0?M68K_RB(base+0x1100)==expected:size==1?M68K_RW(base+0x1100)==expected:((unsigned)M68K_RW(base+0x1100)<<16|M68K_RW(base+0x1102))==expected);
                }
            }
            unsigned base=kind?0xa01000:0xff1000;
            main68k_context.areg[0]=base; opcode(base,3); opcode(0xff0200,0x5250);
            GenesisRAMTransfer_BeginFrame(); run(); GenesisRAMTransfer_EndFrame(60);
            CHECK(latest(kind,0)==(kind?1:2) && latest(kind,1)==(kind?1:2) && M68K_RW(base)==4);
            opcode(0xff0200,0x48d0); opcode(0xff0202,7); opcode(0xff0204,0x4cd0); opcode(0xff0206,7);
            main68k_context.dreg[0]=0x12345678; main68k_context.dreg[1]=0x90abcdef; main68k_context.dreg[2]=0x87654321;
            GenesisRAMTransfer_BeginFrame(); run(); run(0xff0204); GenesisRAMTransfer_EndFrame(60);
            CHECK(latest(kind,0)==(kind?6:12) && latest(kind,1)==(kind?6:12));
            CHECK(main68k_context.dreg[0]==(kind?0x00340078:0x12345678));
            CHECK(main68k_context.dreg[1]==(kind?0x00ab00ef:0x90abcdef));
            opcode(0xff0200,0x2020); opcode(0xff0202,0x2300);
            main68k_context.areg[0]=base+4;main68k_context.areg[1]=base+0x104;
            GenesisRAMTransfer_BeginFrame();run();run(0xff0202);GenesisRAMTransfer_EndFrame(60);
            CHECK(latest(kind,0)==(kind?2:4) && latest(kind,1)==(kind?2:4));
        }
        // Fetch and immediate operands in main RAM do not count; PC-relative data does.
        opcode(0xff0300,0x60fe); GenesisRAMTransfer_BeginFrame();run(0xff0300,100);GenesisRAMTransfer_EndFrame(60);
        CHECK(!latest(0,0) && !latest(0,1));
        opcode(0xff0300,0x203c);opcode(0xff0302,0x1234);opcode(0xff0304,0x5678);
        GenesisRAMTransfer_BeginFrame();run(0xff0300);GenesisRAMTransfer_EndFrame(60);
        CHECK(!latest(0,0) && main68k_context.dreg[0]==0x12345678);
        opcode(0xff0300,0x303a);opcode(0xff0302,0x1e);opcode(0xff0320,0x7654);
        GenesisRAMTransfer_BeginFrame();run(0xff0300);GenesisRAMTransfer_EndFrame(60);
        CHECK(latest(0,0)==2 && (main68k_context.dreg[0]&0xffff)==0x7654);
        opcode(0xff0200,0x4eb9);opcode(0xff0202,0x00ff);opcode(0xff0204,0x0240);opcode(0xff0240,0x4e75);
        main68k_context.areg[7]=0xff5000;
        GenesisRAMTransfer_BeginFrame();run();run(0xff0240);GenesisRAMTransfer_EndFrame(60);
        CHECK(latest(0,0)==4 && latest(0,1)==4 && main68k_context.areg[7]==0xff5000 && main68k_context.pc==0xff0206);
        // Bus request/reset conditions are identical for all three memory maps.
        for(unsigned state=0;state<8;state++) {
            Z80_State=state;Ram_Z80[0x100]=0x12;
            GenesisRAMTransfer_BeginFrame();M68K_Data_RB(0xa00100);M68K_Data_WW(0xa00100,0x3456);GenesisRAMTransfer_EndFrame(60);
            CHECK(latest(1,0)==((state&6)?0:1) && latest(1,1)==((state&6)?0:1));
            CHECK(Ram_Z80[0x100]==((state&6)?0x12:0x56));
        }
        Z80_State=0;
    }
    puts("PASS: native RAM byte/word/long, mirrors, RMW/MOVEM/predecrement, fetch/PC-relative, stack and Z80 bus conditions on Genesis/32X/SegaCD");
    return 0;
}
static int boundary_and_isolation_tests(void) {
    Genesis_Started=1;SegaCD_Started=_32X_Started=CD_32X_Active=0;Init_Memory_M68K(0);Z80_State=0;
    opcode(0xff0200,0x2010);opcode(0xff0202,0x2280);
    const unsigned addresses[]={0xfffffe,0xe0fffe,0xa03ffe,0xa01ffe,0x01ff1000,0x01a00100};
    const unsigned main_counts[]={4,4,0,0,4,0},sub_counts[]={0,0,1,2,0,2};
    for(unsigned i=0;i<6;i++) {
        main68k_context.areg[0]=main68k_context.areg[1]=addresses[i];
        GenesisRAMTransfer_BeginFrame();run();run(0xff0202);GenesisRAMTransfer_EndFrame(60);
        CHECK(latest(0,0)==main_counts[i] && latest(0,1)==main_counts[i]);
        CHECK(latest(1,0)==sub_counts[i] && latest(1,1)==sub_counts[i]);
    }
    GenesisRAMTransfer_BeginFrame();
    M68K_RB(0xff1000);M68K_RW(0xa00100);M68K_WW(0xff1000,0);M68K_WB(0xa00100,0);
    Z80_ReadB(0x100);Z80_WriteB(0x100,0x12);Bank_Z80=0xff0000;Z80_ReadB(0x9000);Z80_WriteB(0x9000,0x34);
    CHECK(M68K_RB(0xff1000)==0x34);
    // DMA source reads from main RAM are attributed only to the DMA graph.
    Set_VDP_Reg(1,0x10);Set_VDP_Reg(15,2);Set_VDP_Reg(19,2);Set_VDP_Reg(20,0);
    Set_VDP_Reg(21,0);Set_VDP_Reg(22,0x88);Set_VDP_Reg(23,0x7f);Write_VDP_Ctrl(0x4000);Write_VDP_Ctrl(0x80);
    M68K_Data_RB(0xa06000);M68K_Data_RW(0xa10000);M68K_Data_RB(0x200000);M68K_Data_WB(0xdfffff,0);
    GenesisRAMTransfer_EndFrame(60);CHECK(!latest(0,0) && !latest(0,1) && !latest(1,0) && !latest(1,1));
    puts("PASS: 24-bit/wrap/boundary splits; debugger, Z80, DMA, ROM and non-RAM I/O excluded");
    return 0;
}
static int history_tests(void) {
    GenesisRAMTransfer_Reset();
    for(unsigned i=0;i<300;i++)sample(i,300-i);
    for(int k=0;k<2;k++) {
        CHECK(GenesisRAMTransfer_GetHistory(k,0,values)==256 && values[0]==44 && values[255]==299);
        CHECK(GenesisRAMTransfer_GetHistory(k,1,values)==256 && values[0]==256 && values[255]==1);
    }
    for(unsigned fps=50;fps<=60;fps+=10) {
        GenesisRAMTransfer_Reset();for(unsigned i=0;i<fps;i++)sample(1024,2048,fps);
        for(int k=0;k<2;k++)CHECK(GenesisRAMTransfer_GetBytesPerSecond(k,0)==fps*1024 && GenesisRAMTransfer_GetBytesPerSecond(k,1)==fps*2048);
        for(unsigned i=0;i<fps;i++)sample(0,0,fps);
        for(int k=0;k<2;k++)CHECK(!GenesisRAMTransfer_GetBytesPerSecond(k,0) && !GenesisRAMTransfer_GetBytesPerSecond(k,1));
    }
    sample(2,4,50);GenesisRAMTransfer_EndFrame(50);CHECK(GenesisRAMTransfer_GetHistory(0,0,values)==1);
    M68K_Reset_CPU();CHECK(!GenesisRAMTransfer_GetHistory(0,0,values) && !GenesisRAMTransfer_GetHistory(1,0,values));
    Genesis_Started=0;sample(2,4);CHECK(!GenesisRAMTransfer_GetHistory(0,0,values));Genesis_Started=1;
    puts("PASS: independent histories/rates, PAL/NTSC, ring rollover, idle decay and reset");
    return 0;
}
static int panel_tests(int kind) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"vrt",0,config));
    Genesis_Started=1; SegaCD_Started=_32X_Started=CD_32X_Active=0; Paused=0;
    GenesisRAMTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    genesisramtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[(DMODE_MAIN_RAM_TRANSFER+kind)-1]; CHECK(panel);
    genesisramtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    genesisramtransfer_update();
    GetDlgItemText(panel,IDC_GENESIS_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_GENESIS_RAM_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_GENESIS_RAM_READ_GRAPH,IDC_GENESIS_RAM_WRITE_GRAPH};
    for (unsigned i=0;i<2;i++) {
        item.hwndItem=GetDlgItem(panel,graphs[i]); CHECK(item.hwndItem);
        SendMessage(panel,WM_DRAWITEM,graphs[i],(LPARAM)&item);
        CHECK(GetPixel(dc,100,40)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,145)==RGB(0,0,255));
        CHECK(GetPixel(dc,100,110)==(i?RGB(0,0,255):GetSysColor(COLOR_WINDOW)));
    }
    // A write burst raises the common axis for both graphs.
    sample(1024,8192); item.hwndItem=GetDlgItem(panel,graphs[0]);
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item);
    CHECK(GetPixel(dc,100,135)==GetSysColor(COLOR_WINDOW));
    Paused=1;
    for (unsigned i=0;i<10;i++) genesisramtransfer_update();
    CHECK(GenesisRAMTransfer_GetHistory(kind,0,values)==256 && latest(kind,0)==1024 && latest(kind,1)==8192);
    GetDlgItemText(panel,IDC_GENESIS_RAM_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"Paused"));
    Genesis_Started=0; genesisramtransfer_update();
    GetDlgItemText(panel,IDC_GENESIS_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; genesisramtransfer_update();
    GetDlgItemText(panel,IDC_GENESIS_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
    RECT client; GetClientRect(panel,&client); MapWindowPoints(panel,NULL,(POINT*)&client,2);
    for (HWND control=GetWindow(panel,GW_CHILD);control;control=GetWindow(control,GW_HWNDNEXT)) {
        RECT bounds, required={0}; GetWindowRect(control,&bounds);
        CHECK(bounds.left>=client.left && bounds.right<=client.right && bounds.bottom<=client.bottom);
        if (control==GetDlgItem(panel,graphs[0]) || control==GetDlgItem(panel,graphs[1])) continue;
        GetWindowText(control,text,sizeof(text)); HDC dc=GetDC(control);
        HGDIOBJ font=SelectObject(dc,(HFONT)SendMessage(control,WM_GETFONT,0,0));
        DrawText(dc,text,-1,&required,DT_CALCRECT|DT_LEFT|DT_NOPREFIX);
        SelectObject(dc,font); ReleaseDC(control,dc);
        CHECK(required.right<=bounds.right-bounds.left && required.bottom<=bounds.bottom-bounds.top);
    }
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(panel,screen);
    OpenWindow_KMod((DMODE_MAIN_RAM_TRANSFER+kind)); GetWindowRect(panel,&saved); genesisramtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !GenesisRAMTransfer_GetHistory(kind,0,values));
    genesisramtransfer_destroy(); genesisramtransfer_create(GetModuleHandle(NULL),NULL); genesisramtransfer_restore_window(config);
    panel=HandleWindow_KMod[(DMODE_MAIN_RAM_TRANSFER+kind)-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    for(int other=0;other<2;other++) if(other!=kind) CHECK(!IsWindowVisible(HandleWindow_KMod[DMODE_MAIN_RAM_TRANSFER+other-1]));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    genesisramtransfer_save_window(config); genesisramtransfer_destroy();
    genesisramtransfer_create(GetModuleHandle(NULL),NULL); genesisramtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[(DMODE_MAIN_RAM_TRANSFER+kind)-1]); genesisramtransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    M68K_Init();S68K_Init();Init_Tab();
    return instruction_tests() || boundary_and_isolation_tests() || history_tests() || panel_tests(0) || panel_tests(1);
}
