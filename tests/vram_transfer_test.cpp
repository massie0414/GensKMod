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
#include "../src/Gens/kmod/vram_transfer_panel.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static unsigned long values[VRAM_TRANSFER_HISTORY];
static unsigned long latest(int write) {
    unsigned count=VRAMTransfer_GetHistory(write,values);
    return count ? values[count-1] : ~0UL;
}
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
static unsigned vword(unsigned address) { return *(unsigned short*)(VRam+(address&0xfffe)); }
static void sample(unsigned reads, unsigned writes, unsigned fps=60) {
    Ctrl.DMA=0; Set_VDP_Reg(15,2); VRAMTransfer_BeginFrame();
    select_vdp(0); for(unsigned i=0;i<reads;i+=2) M68K_Data_RW(0xc00000);
    select_vdp(1); for(unsigned i=0;i<writes;i++) M68K_Data_WB(0xc00000,0);
    VRAMTransfer_EndFrame(fps);
}

static int instruction_tests(void) {
    Genesis_Started=1; SegaCD_Started=_32X_Started=CD_32X_Active=0;
    Rom_Size=0x20000; M68K_Reset(0); Reset_VDP(); Set_VDP_Reg(15,2);
    for(unsigned size=1;size<=2;size++) {
        unsigned bytes=1<<size;
        opcode(0xff0200,size==1?0x3280:0x2280); // MOVE.W/L D0,(A1)
        opcode(0xff0202,size==1?0x3010:0x2010); // MOVE.W/L (A0),D0
        main68k_context.areg[0]=main68k_context.areg[1]=0xc00000;
        main68k_context.dreg[0]=size==1?0x1234:0x12345678;
        VRAMTransfer_Reset(); VRAMTransfer_BeginFrame(); select_vdp(1,0x100);
        run(); CHECK(main68k_context.pc==0xff0202 && Ctrl.Address==0x100+bytes);
        CHECK(vword(0x100)==0x1234 && (size==1 || vword(0x102)==0x5678));
        select_vdp(0,0x100); main68k_context.dreg[0]=0; run(0xff0202); VRAMTransfer_EndFrame(60);
        CHECK(latest(0)==bytes && latest(1)==bytes);
        CHECK(main68k_context.dreg[0]==(size==1?0x1234:0x12345678));
    }
    // Predecrement uses reversed port order; the VDP still auto-increments.
    opcode(0xff0200,0x2300); opcode(0xff0202,0x2020);
    main68k_context.areg[0]=main68k_context.areg[1]=0xc00004; main68k_context.dreg[0]=0x12345678;
    VRAMTransfer_BeginFrame(); select_vdp(1,0x200); run();
    CHECK(vword(0x200)==0x5678 && vword(0x202)==0x1234);
    select_vdp(0,0x200); run(0xff0202); VRAMTransfer_EndFrame(60);
    CHECK(latest(0)==4 && latest(1)==4 && main68k_context.dreg[0]==0x12345678);
    // Byte writes duplicate the byte internally; count one CPU bus byte.
    opcode(0xff0200,0x1280); opcode(0xff0202,0x1010);
    for(unsigned port=0xc00000;port<=0xc00003;port++) {
        main68k_context.areg[0]=main68k_context.areg[1]=port; main68k_context.dreg[0]=0x5a;
        VRAMTransfer_BeginFrame(); select_vdp(1,0x300); run(); CHECK(vword(0x300)==0x5a5a);
        select_vdp(0,0x300); run(0xff0202); VRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==1 && !(main68k_context.dreg[0]&255) && Ctrl.Address==0x300);
    }
    // 24-bit addressing and the second data-port alias.
    opcode(0xff0200,0x3280); opcode(0xff0202,0x3010);
    main68k_context.areg[0]=main68k_context.areg[1]=0x01c00002; main68k_context.dreg[0]=0x1234;
    VRAMTransfer_BeginFrame(); select_vdp(1,0xffff); run();
    CHECK(vword(0xfffe)==0x3412 && Ctrl.Address==1);
    select_vdp(0,0xffff); run(0xff0202); VRAMTransfer_EndFrame(60);
    CHECK(latest(0)==2 && latest(1)==2 && (main68k_context.dreg[0]&0xffff)==0x3412 && Ctrl.Address==1);
    // Same-value stores with auto-increment zero still transfer data.
    Set_VDP_Reg(15,0); VRAMTransfer_BeginFrame(); select_vdp(1,0x400);
    run(); run(); run(); VRAMTransfer_EndFrame(60); CHECK(latest(1)==6 && Ctrl.Address==0x400);
    Set_VDP_Reg(15,2);
    // A longword at $C00002 has one data word and one control/status word.
    opcode(0xff0200,0x2280); opcode(0xff0202,0x2010);
    main68k_context.areg[0]=main68k_context.areg[1]=0xc00002; main68k_context.dreg[0]=0x12340000;
    VRAMTransfer_BeginFrame(); select_vdp(1,0x500); run();
    Ctrl.Flag=0; select_vdp(0,0x500); run(0xff0202); VRAMTransfer_EndFrame(60);
    CHECK(latest(0)==2 && latest(1)==2);
    // VRAM reads in a read/modify/write instruction do not make its rejected write count.
    opcode(0xff0200,0x5250); main68k_context.areg[0]=0xc00000;
    VRAMTransfer_BeginFrame(); select_vdp(0,0x500); run(); VRAMTransfer_EndFrame(60);
    CHECK(latest(0)==2 && latest(1)==0 && vword(0x500)==0x1234);
    puts("PASS: native VDP word/long/predecrement and byte writes; CPU values, aliases, wrap, auto-increment and access direction");
    return 0;
}

static int isolation_and_history_tests(void) {
    VRAMTransfer_Reset(); Ctrl.Flag=Ctrl.DMA=0; Set_VDP_Reg(15,2);
    select_vdp(1); M68K_Data_WW(0xc00000,7); VRAMTransfer_EndFrame(60);
    CHECK(!VRAMTransfer_GetHistory(0,values));
    VRAMTransfer_BeginFrame();
    const unsigned other_codes[]={3,4,5,8,7,9,13,15};
    for(unsigned i=0;i<sizeof(other_codes)/sizeof(other_codes[0]);i++) {
        select_vdp(other_codes[i]); CHECK(Ctrl.Access!=5 && Ctrl.Access!=9); M68K_Data_RW(0xc00000); M68K_Data_WB(0xc00000,1); M68K_Data_WW(0xc00000,2);
    }
    select_vdp(0); M68K_Data_RW(0xc00004); M68K_Data_RW(0xc00008); M68K_Data_RW(0xc00020);
    select_vdp(1); M68K_Data_WW(0xc00020,0); M68K_Data_WB(0xc00004,0);
    // Raw debugger helpers, renderer access and Z80 data-port writes are separate.
    select_vdp(0); M68K_RW(0xc00000); Read_VDP_Data();
    select_vdp(1); M68K_WB(0xc00000,0); M68K_WW(0xc00000,0); Write_Word_VDP_Data(0);
    select_vdp(1,0x600); Z80_WriteB(0x7f00,0x56); CHECK(vword(0x600)==0x5656);
    VRAMTransfer_EndFrame(60); CHECK(latest(0)==0 && latest(1)==0);
    // Real CPU-triggered memory-to-VRAM DMA, fill and copy all remain excluded.
    VRAMTransfer_BeginFrame(); opcode(0xff1000,0x1234); opcode(0xff1002,0x5678);
    Set_VDP_Reg(1,0x10); Set_VDP_Reg(15,2); Set_VDP_Reg(19,2); Set_VDP_Reg(20,0);
    Set_VDP_Reg(21,0); Set_VDP_Reg(22,0x88); Set_VDP_Reg(23,0x7f); select_vdp(0x21,0x2000);
    CHECK(vword(0x2000)==0x1234 && vword(0x2002)==0x5678);
    Set_VDP_Reg(15,1); Set_VDP_Reg(19,4); Set_VDP_Reg(20,0);
    Set_VDP_Reg(21,0); Set_VDP_Reg(22,0x20); Set_VDP_Reg(23,0xc0); select_vdp(0x21,0x2100);
    CHECK(!memcmp(VRam+0x2000,VRam+0x2100,4));
    Set_VDP_Reg(19,3); Set_VDP_Reg(20,0); Set_VDP_Reg(23,0x80); select_vdp(0x21,0x2200);
    CHECK(Ctrl.DMA&4); M68K_Data_WW(0xc00000,0xabcd);
    CHECK(!(Ctrl.DMA&4) && VRam[0x2200]==0xab);
    VRAMTransfer_EndFrame(60); CHECK(latest(0)==0 && latest(1)==0);
    // A normal store after a fill must count again.
    VRAMTransfer_BeginFrame(); M68K_Data_WW(0xc00000,0x1234); VRAMTransfer_EndFrame(60); CHECK(latest(1)==2);
    for(unsigned i=0;i<300;i++)sample(i*2,(300-i)*2);
    CHECK(VRAMTransfer_GetHistory(0,values)==256 && values[0]==88 && values[255]==598);
    CHECK(VRAMTransfer_GetHistory(1,values)==256 && values[0]==512 && values[255]==2);
    for(unsigned fps=50;fps<=60;fps+=10) {
        VRAMTransfer_Reset();
        for(unsigned i=0;i<fps;i++)sample(1024,2048,fps);
        CHECK(VRAMTransfer_GetBytesPerSecond(0)==fps*1024 && VRAMTransfer_GetBytesPerSecond(1)==fps*2048);
        for(unsigned i=0;i<fps;i++)sample(0,0,fps);
        CHECK(!VRAMTransfer_GetBytesPerSecond(0) && !VRAMTransfer_GetBytesPerSecond(1));
    }
    sample(2,4,50); CHECK(VRAMTransfer_GetHistory(0,values)==1);
    VRAMTransfer_EndFrame(50); CHECK(VRAMTransfer_GetHistory(0,values)==1);
    M68K_Reset_CPU(); CHECK(!VRAMTransfer_GetHistory(0,values));
    sample(2,4); M68K_Reset(0); CHECK(!VRAMTransfer_GetHistory(1,values));
    Genesis_Started=0; sample(2,4); CHECK(!VRAMTransfer_GetHistory(0,values)); Genesis_Started=1;
    puts("PASS: CRAM/VSRAM/control/invalid ports, debugger, Z80, memory DMA/fill/copy excluded; histories, PAL/NTSC, idle decay/reset");
    return 0;
}

static int panel_tests(void) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"vrt",0,config));
    Genesis_Started=1; SegaCD_Started=_32X_Started=CD_32X_Active=0; Paused=0;
    VRAMTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    vramtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_VRAM_TRANSFER-1]; CHECK(panel);
    vramtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    vramtransfer_update();
    GetDlgItemText(panel,IDC_VRAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_VRAM_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_VRAM_READ_GRAPH,IDC_VRAM_WRITE_GRAPH};
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
    for (unsigned i=0;i<10;i++) vramtransfer_update();
    CHECK(VRAMTransfer_GetHistory(0,values)==256 && latest(0)==1024 && latest(1)==8192);
    GetDlgItemText(panel,IDC_VRAM_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"64 KiB") && strstr(text,"Paused"));
    Genesis_Started=0; vramtransfer_update();
    GetDlgItemText(panel,IDC_VRAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; vramtransfer_update();
    GetDlgItemText(panel,IDC_VRAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
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
    OpenWindow_KMod(DMODE_VRAM_TRANSFER); GetWindowRect(panel,&saved); vramtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !VRAMTransfer_GetHistory(0,values));
    vramtransfer_destroy(); vramtransfer_create(GetModuleHandle(NULL),NULL); vramtransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_VRAM_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    vramtransfer_save_window(config); vramtransfer_destroy();
    vramtransfer_create(GetModuleHandle(NULL),NULL); vramtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_VRAM_TRANSFER-1]); vramtransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    M68K_Init(); S68K_Init(); Init_Tab();
    return instruction_tests() || isolation_and_history_tests() || panel_tests();
}
