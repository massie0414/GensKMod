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
#include "../src/Gens/main_word_ram_transfer.h"
#include "../src/Gens/Mem_Z80.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/vdp_transfer_panel.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static void opcode(unsigned address, unsigned value) { M68K_WW(address,(unsigned short)value); }
static void run(unsigned pc=0xff0200, unsigned cycles=1) {
    main68k_context.pc=pc; main68k_context.sr=0x2700;
    memset(main68k_context.interrupts,0,sizeof(main68k_context.interrupts));
    main68k_tripOdometer(); main68k_exec(cycles);
}
// Real VDP commands: code 0/1 = VRAM read/write, 8/3 = CRAM, 4/5 = VSRAM.
static void select_vdp(unsigned code, unsigned address=0) {
    M68K_Data_WW(0xc00004,(address&0x3fff)|((code&3)<<14));
    M68K_Data_WW(0xc00006,(address>>14)|((code&0x3c)<<2));
}

static unsigned long values[VRAM_TRANSFER_HISTORY];
static unsigned long latest(int kind,int write) {
    unsigned n=VDPTransfer_GetHistory(kind,write,values);
    return n?values[n-1]:~0UL;
}
static void memory_dma(unsigned code,unsigned dest,unsigned source,unsigned words) {
    Set_VDP_Reg(1,0x10); Set_VDP_Reg(15,2);
    Set_VDP_Reg(19,words&255); Set_VDP_Reg(20,words>>8);
    Set_VDP_Reg(21,(source>>1)&255); Set_VDP_Reg(22,(source>>9)&255); Set_VDP_Reg(23,(source>>17)&127);
    select_vdp(code|0x20,dest);
}
static void fill(unsigned dest,unsigned length,unsigned inc=1) {
    Set_VDP_Reg(1,0x10); Set_VDP_Reg(15,inc);
    Set_VDP_Reg(19,length&255); Set_VDP_Reg(20,length>>8); Set_VDP_Reg(23,0x80);
    select_vdp(0x21,dest); M68K_Data_WW(0xc00000,0xabcd);
}
static int cpu_tests(void) {
    Genesis_Started=1; SegaCD_Started=_32X_Started=CD_32X_Active=0;
    Rom_Size=0x20000; M68K_Reset(0); Reset_VDP(); Set_VDP_Reg(15,2);
    for(int kind=VDP_TRANSFER_CRAM;kind<=VDP_TRANSFER_VSRAM;kind++) {
        unsigned rd=kind==VDP_TRANSFER_CRAM?8:4,wr=kind==VDP_TRANSFER_CRAM?3:5;
        unsigned char *ram=kind==VDP_TRANSFER_CRAM?CRam:VSRam;
        for(unsigned size=1;size<=2;size++) {
            opcode(0xff0200,size==1?0x3280:0x2280); opcode(0xff0202,size==1?0x3010:0x2010);
            main68k_context.areg[0]=main68k_context.areg[1]=0x01c00000;
            main68k_context.dreg[0]=size==1?0x1234:0x12345678;
            VRAMTransfer_Reset(); VRAMTransfer_BeginFrame(); select_vdp(wr,0x7e); run();
            CHECK(*(unsigned short*)(ram+0x7e)==0x1234);
            CHECK(size==1 || *(unsigned short*)ram==0x5678);
            select_vdp(rd,0x7e); main68k_context.dreg[0]=0; run(0xff0202); VRAMTransfer_EndFrame(60);
            CHECK(latest(kind,0)==(1U<<size) && latest(kind,1)==(1U<<size));
            CHECK(main68k_context.dreg[0]==(size==1?0x1234:0x12345678));
            CHECK(latest(3-kind,0)==0 && latest(3-kind,1)==0 && latest(VDP_TRANSFER_VRAM,1)==0 && latest(VDP_TRANSFER_DMA,1)==0);
        }
        opcode(0xff0200,0x1280); opcode(0xff0202,0x1010);
        for(unsigned port=0xc00000;port<=0xc00003;port++) {
            main68k_context.areg[0]=main68k_context.areg[1]=port; main68k_context.dreg[0]=0x5a;
            VRAMTransfer_BeginFrame(); select_vdp(wr,0xffff); run(); CHECK(*(unsigned short*)(ram+0x7e)==0x5a5a);
            select_vdp(rd,0xffff); run(0xff0202); VRAMTransfer_EndFrame(60);
            CHECK(latest(kind,0)==0 && latest(kind,1)==1 && Ctrl.Address==0xffff);
        }
        // Raw/debugger and Z80 accesses never enter the CPU channel.
        VRAMTransfer_BeginFrame(); select_vdp(rd); M68K_RW(0xc00000); Read_VDP_Data();
        select_vdp(wr); M68K_WW(0xc00000,0); Write_Word_VDP_Data(0); Z80_WriteB(0x7f00,0x34);
        select_vdp(rd); M68K_Data_WW(0xc00000,0); // rejected direction
        select_vdp(wr); M68K_Data_RW(0xc00000);
        M68K_Data_WW(0xc00020,0); M68K_Data_RW(0xc00008);
        VRAMTransfer_EndFrame(60); CHECK(latest(kind,0)==0 && latest(kind,1)==0);
        Set_VDP_Reg(15,0); VRAMTransfer_BeginFrame(); select_vdp(wr);
        M68K_Data_WW(0xc00002,0x4321); M68K_Data_WW(0xc00002,0x4321);
        select_vdp(rd); CHECK(M68K_Data_RW(0xc00002)==0x4321);
        VRAMTransfer_EndFrame(60); CHECK(latest(kind,0)==2 && latest(kind,1)==4);
        Set_VDP_Reg(15,2);
    }
    puts("PASS: native CRAM/VSRAM word/long/byte writes, values, aliases, wrap and independent CPU counters");
    return 0;
}
static int dma_tests(void) {
    opcode(0xff1000,0x1234); opcode(0xff1002,0x5678);
    for(unsigned target=0;target<3;target++) {
        unsigned code=target==0?1:target==1?3:5;
        unsigned char *ram=target==0?VRam:target==1?CRam:VSRam;
        VRAMTransfer_Reset(); VRAMTransfer_BeginFrame(); memory_dma(code,0,0xff1000,2);
        CHECK(*(unsigned short*)ram==0x1234 && *(unsigned short*)(ram+2)==0x5678);
        VRAMTransfer_EndFrame(60); CHECK(latest(VDP_TRANSFER_DMA,0)==4 && latest(VDP_TRANSFER_DMA,1)==4);
        for(int k=0;k<3;k++) CHECK(latest(k,0)==0 && latest(k,1)==0);
        VRAMTransfer_BeginFrame(); for(int i=0;i<10;i++)Update_DMA(); VRAMTransfer_EndFrame(60);
        CHECK(latest(VDP_TRANSFER_DMA,0)==0 && latest(VDP_TRANSFER_DMA,1)==0);
    }
    VRam[0xffff]=0x12; VRam[0]=0x34;
    VRAMTransfer_BeginFrame(); Set_VDP_Reg(15,1); Set_VDP_Reg(19,2); Set_VDP_Reg(20,0);
    Set_VDP_Reg(21,255); Set_VDP_Reg(22,255); Set_VDP_Reg(23,0xc0); select_vdp(0x21,0x100);
    CHECK(VRam[0x100]==0x12 && VRam[0x101]==0x34);
    VRAMTransfer_EndFrame(60); CHECK(latest(VDP_TRANSFER_DMA,0)==2 && latest(VDP_TRANSFER_DMA,1)==2);
    VRAMTransfer_BeginFrame(); fill(0x200,3);
    CHECK(VRam[0x200]==0xab && VRam[0x203]==0xab && Ctrl.Address==0x204);
    VRAMTransfer_EndFrame(60); CHECK(latest(VDP_TRANSFER_DMA,0)==0 && latest(VDP_TRANSFER_DMA,1)==5);
    // Count the emulator's actual stores: seed + (length + 1); zero fills 65536 bytes plus seed.
    VRAMTransfer_BeginFrame(); fill(0x300,0,0); VRAMTransfer_EndFrame(60);
    CHECK(latest(VDP_TRANSFER_DMA,0)==0 && latest(VDP_TRANSFER_DMA,1)==65537 && Ctrl.Address==0x300);
    VRAMTransfer_BeginFrame(); memory_dma(1,0,0xff1000,0); VRAMTransfer_EndFrame(60);
    CHECK(latest(VDP_TRANSFER_DMA,0)==0 && latest(VDP_TRANSFER_DMA,1)==0); // existing zero-length memory-DMA behavior
    VRAMTransfer_BeginFrame(); Set_VDP_Reg(1,0); Set_VDP_Reg(19,2); select_vdp(0x21);
    VRAMTransfer_EndFrame(60); CHECK(latest(VDP_TRANSFER_DMA,1)==0);
    // Refused Word RAM bank access must not be mistaken for a completed DMA.
    SegaCD_Started=1; Ram_Word_State=1; Init_Memory_M68K(2);
    VRAMTransfer_BeginFrame(); memory_dma(1,0x400,0x200002,2); VRAMTransfer_EndFrame(60);
    CHECK(latest(VDP_TRANSFER_DMA,1)==0);
    SegaCD_Started=0; Init_Memory_M68K(0);
    puts("PASS: DMA to all VDP memories, copy wrap, fill/zero length, disabled/rejected DMA and no timing double count");
    return 0;
}
static void sample(unsigned reads,unsigned writes,unsigned fps=60) {
    Ctrl.DMA=0; Set_VDP_Reg(15,2); VRAMTransfer_BeginFrame();
    for(int kind=1;kind<=2;kind++) {
        select_vdp(kind==1?8:4); for(unsigned i=0;i<reads;i+=2) M68K_Data_RW(0xc00000);
        select_vdp(kind==1?3:5); for(unsigned i=0;i<writes;i++) M68K_Data_WB(0xc00000,0);
    }
    if(reads) memory_dma(1,0x1000,0xff1000,reads/2);
    if(writes>reads) fill(0x2000,writes-reads-2);
    VRAMTransfer_EndFrame(fps);
}
static int history_tests(void) {
    VRAMTransfer_Reset();
    for(unsigned i=0;i<300;i++)sample(i*2,i*2+8);
    for(int kind=1;kind<=3;kind++) {
        CHECK(VDPTransfer_GetHistory(kind,0,values)==256 && values[0]==88 && values[255]==598);
        CHECK(VDPTransfer_GetHistory(kind,1,values)==256 && values[0]==96 && values[255]==606);
    }
    for(unsigned fps=50;fps<=60;fps+=10) {
        VRAMTransfer_Reset(); for(unsigned i=0;i<fps;i++)sample(1024,2048,fps);
        for(int kind=1;kind<=3;kind++) CHECK(VDPTransfer_GetBytesPerSecond(kind,0)==fps*1024 && VDPTransfer_GetBytesPerSecond(kind,1)==fps*2048);
        for(unsigned i=0;i<fps;i++)sample(0,0,fps);
        for(int kind=1;kind<=3;kind++) CHECK(!VDPTransfer_GetBytesPerSecond(kind,0) && !VDPTransfer_GetBytesPerSecond(kind,1));
    }
    sample(2,4,50); VRAMTransfer_EndFrame(50);
    for(int kind=1;kind<=3;kind++) CHECK(VDPTransfer_GetHistory(kind,0,values)==1);
    M68K_Reset_CPU(); for(int kind=1;kind<=3;kind++) CHECK(!VDPTransfer_GetHistory(kind,0,values));
    Genesis_Started=0; sample(2,4); for(int kind=1;kind<=3;kind++) CHECK(!VDPTransfer_GetHistory(kind,0,values));
    Genesis_Started=1;
    puts("PASS: independent histories, ring rollover, PAL/NTSC rates, idle decay, reset and inactive frames");
    return 0;
}
static int panel_tests(int kind) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"vrt",0,config));
    Genesis_Started=1; SegaCD_Started=_32X_Started=CD_32X_Active=0; Paused=0;
    VRAMTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    vdptransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[(DMODE_CRAM_TRANSFER+kind-1)-1]; CHECK(panel);
    vdptransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    vdptransfer_update();
    GetDlgItemText(panel,IDC_VDP_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_VDP_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_VDP_READ_GRAPH,IDC_VDP_WRITE_GRAPH};
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
    for (unsigned i=0;i<10;i++) vdptransfer_update();
    CHECK(VDPTransfer_GetHistory(kind,0,values)==256 && latest(kind,0)==1024 && latest(kind,1)==8192);
    GetDlgItemText(panel,IDC_VDP_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"Paused"));
    Genesis_Started=0; vdptransfer_update();
    GetDlgItemText(panel,IDC_VDP_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; vdptransfer_update();
    GetDlgItemText(panel,IDC_VDP_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
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
    OpenWindow_KMod((DMODE_CRAM_TRANSFER+kind-1)); GetWindowRect(panel,&saved); vdptransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !VDPTransfer_GetHistory(kind,0,values));
    vdptransfer_destroy(); vdptransfer_create(GetModuleHandle(NULL),NULL); vdptransfer_restore_window(config);
    panel=HandleWindow_KMod[(DMODE_CRAM_TRANSFER+kind-1)-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    for(int other=1;other<=3;other++) if(other!=kind) CHECK(!IsWindowVisible(HandleWindow_KMod[DMODE_CRAM_TRANSFER+other-2]));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    vdptransfer_save_window(config); vdptransfer_destroy();
    vdptransfer_create(GetModuleHandle(NULL),NULL); vdptransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[(DMODE_CRAM_TRANSFER+kind-1)-1]); vdptransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    M68K_Init(); S68K_Init(); Init_Tab();
    return cpu_tests() || dma_tests() || history_tests() || panel_tests(1) || panel_tests(2) || panel_tests(3);
}
