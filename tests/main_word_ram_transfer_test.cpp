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
#include "../src/Gens/main_word_ram_transfer.h"
#include "../src/Gens/word_ram_transfer.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/main_word_ram_transfer_panel.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static unsigned long values[MAIN_WORD_RAM_TRANSFER_HISTORY];
static unsigned long latest(int write) {
    unsigned count=MainWordRAMTransfer_GetHistory(write,values);
    return count ? values[count-1] : ~0UL;
}
static void opcode(unsigned address, unsigned value) { M68K_WW(address,(unsigned short)value); }
static void run(unsigned pc=0xff0200, unsigned cycles=1) {
    main68k_context.pc=pc; main68k_context.sr=0x2700;
    memset(main68k_context.interrupts,0,sizeof(main68k_context.interrupts));
    main68k_tripOdometer(); main68k_exec(cycles);
}
static void sample(unsigned reads, unsigned writes, unsigned fps=60) {
    MainWordRAMTransfer_BeginFrame();
    for (unsigned i=0;i<reads;i++) M68K_Data_RB(0x200100);
    for (unsigned i=0;i<writes;i++) M68K_Data_WB(0x200200,0);
    MainWordRAMTransfer_EndFrame(fps);
}

static int instruction_tests(void) {
    const unsigned readops[]={0x1010,0x3010,0x2010}; // MOVE.B/W/L (A0),D0
    const unsigned writeops[]={0x1280,0x3280,0x2280}; // MOVE.B/W/L D0,(A1)
    S68K_Reset(); Rom_Size=0x20000; M68K_Reset(2); SegaCD_Started=1; CD_32X_Active=0;
    for (unsigned mode=0;mode<4;mode++) {
        Ram_Word_State=mode; MS68K_Set_Word_Ram();
        for (unsigned view=0;view<(mode<2?1U:2U);view++) {
            unsigned base=view?0x220000:0x200000;
            for (unsigned size=0;size<3;size++) {
                unsigned bytes=1<<size, expected=size==0?0x12:size==1?0x1234:0x12345678;
                opcode(base+0x100,0x1234); opcode(base+0x102,0x5678);
                if(!size) expected=M68K_RB(base+0x100); // Preserve the existing cell-view byte ordering.
                opcode(0xff0200,readops[size]); opcode(0xff0202,writeops[size]);
                main68k_context.areg[0]=base+0x100; main68k_context.areg[1]=base+0x200;
                main68k_context.dreg[0]=0;
                MainWordRAMTransfer_Reset(); MainWordRAMTransfer_BeginFrame();
                run(); CHECK(main68k_context.pc==0xff0202 && main68k_context.dreg[0]==expected);
                run(0xff0202); MainWordRAMTransfer_EndFrame(60);
                CHECK(latest(0)==bytes && latest(1)==bytes);
                CHECK((bytes==1 && M68K_RB(base+0x200)==expected) ||
                    (bytes==2 && M68K_RW(base+0x200)==expected) ||
                    (bytes==4 && M68K_RW(base+0x200)==0x1234 && M68K_RW(base+0x202)==0x5678));
            }
            opcode(0xff0200,0x5250); opcode(base+0x100,3); // ADDQ.W #1,(A0)
            MainWordRAMTransfer_BeginFrame(); run(); MainWordRAMTransfer_EndFrame(60);
            CHECK(latest(0)==2 && latest(1)==2 && M68K_RW(base+0x100)==4);
            opcode(0xff0200,0x48d0); opcode(0xff0202,7); // MOVEM.L D0-D2,(A0)
            opcode(0xff0204,0x4cd0); opcode(0xff0206,7); // MOVEM.L (A0),D0-D2
            main68k_context.dreg[0]=0x12345678; main68k_context.dreg[1]=0x90abcdef; main68k_context.dreg[2]=0x87654321;
            MainWordRAMTransfer_BeginFrame(); run();
            main68k_context.dreg[0]=main68k_context.dreg[1]=main68k_context.dreg[2]=0;
            run(0xff0204); MainWordRAMTransfer_EndFrame(60);
            CHECK(latest(0)==12 && latest(1)==12);
            CHECK(main68k_context.dreg[0]==0x12345678 && main68k_context.dreg[1]==0x90abcdef && main68k_context.dreg[2]==0x87654321);
            // Predecrement longwords use separate generator helpers/order.
            opcode(0xff0200,0x2020); opcode(0xff0202,0x2300);
            main68k_context.areg[0]=base+0x104; main68k_context.areg[1]=base+0x204;
            MainWordRAMTransfer_BeginFrame(); run(); run(0xff0202); MainWordRAMTransfer_EndFrame(60);
            CHECK(latest(0)==4 && latest(1)==4 && main68k_context.dreg[0]==0x12345678);
            CHECK(M68K_RW(base+0x200)==0x1234 && M68K_RW(base+0x202)==0x5678);
            if (view) {
                // Confirm the cell view reached the active physical bank.
                unsigned offset=Cell_Conv_Tab[0x200/2]*2+((mode&1)?0x20000:0);
                CHECK(*(unsigned short*)(Ram_Word_1M+offset)==0x1234);
            }
        }
        opcode(0x200300,0x60fe);
        MainWordRAMTransfer_BeginFrame(); run(0x200300,200); MainWordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==0); // Fetches excluded, even in Word RAM.
        opcode(0x200300,0x203c); opcode(0x200302,0x1234); opcode(0x200304,0x5678);
        MainWordRAMTransfer_BeginFrame(); run(0x200300); MainWordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==0 && main68k_context.dreg[0]==0x12345678);
        opcode(0x200300,0x303a); opcode(0x200302,0x1e); opcode(0x200320,0x7654);
        MainWordRAMTransfer_BeginFrame(); run(0x200300); MainWordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==2 && latest(1)==0 && (main68k_context.dreg[0]&0xffff)==0x7654);
        opcode(0xff0200,0x2010); opcode(0xff0202,0x2280);
        main68k_context.areg[0]=main68k_context.areg[1]=0x23fffe;
        opcode(0x23fffe,0xbeef);
        MainWordRAMTransfer_BeginFrame(); run(); run(0xff0202); MainWordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==2 && latest(1)==2); // Second word is unmapped.
        main68k_context.areg[0]=main68k_context.areg[1]=0x21fffe;
        MainWordRAMTransfer_BeginFrame(); run(); run(0xff0202); MainWordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==4 && latest(1)==4); // Linear/cell boundary is fully mapped.
    }
    main68k_context.areg[0]=0x01200100; main68k_context.areg[1]=0x01200200;
    MainWordRAMTransfer_BeginFrame(); run(); run(0xff0202); MainWordRAMTransfer_EndFrame(60);
    CHECK(latest(0)==4 && latest(1)==4); // CPU's 24-bit address mask.
    for (unsigned i=0;i<3;i++) {
        const unsigned addresses[]={0xff6000,0x240000,0x020000};
        main68k_context.areg[0]=main68k_context.areg[1]=addresses[i];
        MainWordRAMTransfer_BeginFrame(); run(); run(0xff0202); MainWordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==0);
    }
    puts("PASS: main-68000 byte/word/long, predecrement, MOVEM/RMW, PC-relative data, 1M cell/banks, 2M, boundaries and fetch exclusion");
    return 0;
}

static int isolation_and_history_tests(void) {
    Ram_Word_State=0; MS68K_Set_Word_Ram();
    MainWordRAMTransfer_Reset();
    M68K_Data_RB(0x200100); M68K_Data_WW(0x200200,7); MainWordRAMTransfer_EndFrame(60);
    CHECK(!MainWordRAMTransfer_GetHistory(0,values));
    MainWordRAMTransfer_BeginFrame(); WordRAMTransfer_Reset(); WordRAMTransfer_BeginFrame();
    M68K_RB(0x200100); M68K_RW(0x200100); M68K_WB(0x200200,1); M68K_WW(0x200200,2);
    S68K_Data_RB(0x80100); S68K_Data_RW(0x80100); S68K_Data_WW(0x80200,3);
    WordRAMTransfer_EndFrame(60);
    CHECK(WordRAMTransfer_GetHistory(0,values)==1 && values[0]==3);
    CHECK(WordRAMTransfer_GetHistory(1,values)==1 && values[0]==2);
    CDC.RS0=0x700; CDC.DMA_Adr=0; CDC.DBC.N=4; CDC.DAC.N=0; CDC.IFCTRL=0;
    CDC.Buffer[0]=0x12; CDC.Buffer[1]=0x34; SCD.Status_CDC=8;
    Update_CDC_TRansfert(); CHECK(M68K_RW(0x200000)==0x1234);
    // The VDP really copies Word RAM, but its DMA is not main-CPU data traffic.
    Rom_Size=0x20000; opcode(0x200100,0x4567); opcode(0x200102,0x89ab);
    Set_VDP_Reg(1,0x10); Set_VDP_Reg(15,2); Set_VDP_Reg(19,2); Set_VDP_Reg(20,0);
    Set_VDP_Reg(21,0x81); Set_VDP_Reg(22,0); Set_VDP_Reg(23,0x10);
    Write_VDP_Ctrl(0x4000); Write_VDP_Ctrl(0x80);
    CHECK(*(unsigned short*)VRam==0x4567 && *(unsigned short*)(VRam+2)==0x89ab);
    MainWordRAMTransfer_EndFrame(60); CHECK(latest(0)==0 && latest(1)==0);
    // The same CPU addresses are cartridge ROM without the CD add-on.
    SegaCD_Started=0; Init_Memory_M68K(0); MainWordRAMTransfer_Reset();
    MainWordRAMTransfer_BeginFrame(); M68K_Data_RW(0x200000); M68K_Data_WW(0x200000,1);
    MainWordRAMTransfer_EndFrame(60); CHECK(!MainWordRAMTransfer_GetHistory(0,values));
    SegaCD_Started=1; Init_Memory_M68K(2);
    for (unsigned i=0;i<300;i++) sample(i,300-i);
    CHECK(MainWordRAMTransfer_GetHistory(0,values)==256 && values[0]==44 && values[255]==299);
    CHECK(MainWordRAMTransfer_GetHistory(1,values)==256 && values[0]==256 && values[255]==1);
    for (unsigned fps=50;fps<=60;fps+=10) {
        MainWordRAMTransfer_Reset();
        for (unsigned i=0;i<fps;i++) sample(1024,2048,fps);
        CHECK(MainWordRAMTransfer_GetBytesPerSecond(0)==fps*1024 && MainWordRAMTransfer_GetBytesPerSecond(1)==fps*2048);
        for (unsigned i=0;i<fps;i++) sample(0,0,fps);
        CHECK(!MainWordRAMTransfer_GetBytesPerSecond(0) && !MainWordRAMTransfer_GetBytesPerSecond(1));
    }
    sample(2,4,50); CHECK(MainWordRAMTransfer_GetHistory(0,values)==1);
    MainWordRAMTransfer_EndFrame(50); CHECK(MainWordRAMTransfer_GetHistory(0,values)==1);
    M68K_Reset_CPU(); CHECK(!MainWordRAMTransfer_GetHistory(0,values));
    sample(2,4); M68K_Reset(2); CHECK(!MainWordRAMTransfer_GetHistory(1,values));
    puts("PASS: debugger/sub CPU/CDC and VDP DMA excluded; cartridge addresses excluded; independent histories and rates, PAL/NTSC, idle decay and CPU reset");
    return 0;
}

static int panel_tests(void) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"mrt",0,config));
    SegaCD_Started=1; CD_32X_Active=0; Paused=0; Ram_Word_State=1;
    MainWordRAMTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    mainwordramtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER-1]; CHECK(panel);
    mainwordramtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    mainwordramtransfer_update();
    GetDlgItemText(panel,IDC_MAIN_WORD_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_MAIN_WORD_RAM_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_MAIN_WORD_RAM_READ_GRAPH,IDC_MAIN_WORD_RAM_WRITE_GRAPH};
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
    Paused=1; Ram_Word_State=2;
    for (unsigned i=0;i<10;i++) mainwordramtransfer_update();
    CHECK(MainWordRAMTransfer_GetHistory(0,values)==256 && latest(0)==1024 && latest(1)==8192);
    GetDlgItemText(panel,IDC_MAIN_WORD_RAM_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"bank 0") && strstr(text,"Paused"));
    SegaCD_Started=0; mainwordramtransfer_update();
    GetDlgItemText(panel,IDC_MAIN_WORD_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; mainwordramtransfer_update();
    GetDlgItemText(panel,IDC_MAIN_WORD_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
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
    OpenWindow_KMod(DMODE_MAIN_WORD_RAM_TRANSFER); GetWindowRect(panel,&saved); mainwordramtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !MainWordRAMTransfer_GetHistory(0,values));
    mainwordramtransfer_destroy(); mainwordramtransfer_create(GetModuleHandle(NULL),NULL); mainwordramtransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    mainwordramtransfer_save_window(config); mainwordramtransfer_destroy();
    mainwordramtransfer_create(GetModuleHandle(NULL),NULL); mainwordramtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_MAIN_WORD_RAM_TRANSFER-1]); mainwordramtransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    M68K_Init(); S68K_Init(); Init_Tab();
    return instruction_tests() || isolation_and_history_tests() || panel_tests();
}
