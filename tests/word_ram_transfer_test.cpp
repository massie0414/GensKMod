// Native sub-68000 instructions and real dialog resources; no game/BIOS needed.
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
#include "../src/Gens/word_ram_transfer.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/word_ram_transfer_panel.h"

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
static unsigned long values[WORD_RAM_TRANSFER_HISTORY];
static unsigned long latest(int write) {
    unsigned count=WordRAMTransfer_GetHistory(write,values);
    return count ? values[count-1] : ~0UL;
}
static void opcode(unsigned address, unsigned value) { S68K_WW(address,(unsigned short)value); }
static void run(unsigned pc=0x200, unsigned cycles=1) {
    sub68k_context.pc=pc; sub68k_context.sr=0x2700;
    memset(sub68k_context.interrupts,0,sizeof(sub68k_context.interrupts));
    sub68k_tripOdometer(); sub68k_exec(cycles);
}
static void sample(unsigned reads, unsigned writes, unsigned fps=60) {
    WordRAMTransfer_BeginFrame();
    for (unsigned i=0;i<reads;i++) S68K_Data_RB(0x080100);
    for (unsigned i=0;i<writes;i++) S68K_Data_WB(0x080200,0);
    WordRAMTransfer_EndFrame(fps);
}

static int instruction_tests(void) {
    const unsigned readops[]={0x1010,0x3010,0x2010}; // MOVE.B/W/L (A0),D0
    const unsigned writeops[]={0x1280,0x3280,0x2280}; // MOVE.B/W/L D0,(A1)
    S68K_Reset();
    for (unsigned mode=0;mode<4;mode++) {
        Ram_Word_State=mode; MS68K_Set_Word_Ram();
        unsigned base=mode<2?0x080000:0x0c0000;
        for (unsigned size=0;size<3;size++) {
            unsigned bytes=1<<size, expected=size==0?0x12:size==1?0x1234:0x12345678;
            opcode(base+0x100,0x1234); opcode(base+0x102,0x5678);
            opcode(0x200,readops[size]); opcode(0x202,writeops[size]);
            sub68k_context.areg[0]=base+0x100; sub68k_context.areg[1]=base+0x200;
            sub68k_context.dreg[0]=0;
            WordRAMTransfer_Reset(); WordRAMTransfer_BeginFrame();
            run(); CHECK(sub68k_context.pc==0x202 && sub68k_context.dreg[0]==expected);
            run(0x202); WordRAMTransfer_EndFrame(60);
            CHECK(latest(0)==bytes && latest(1)==bytes);
            CHECK((bytes==1 && S68K_RB(base+0x200)==expected) ||
                (bytes==2 && S68K_RW(base+0x200)==expected) ||
                (bytes==4 && S68K_RW(base+0x200)==0x1234 && S68K_RW(base+0x202)==0x5678));
        }
        // Read/modify/write and repeated same-value writes are bus traffic.
        opcode(0x200,0x5250); opcode(base+0x100,3); // ADDQ.W #1,(A0)
        WordRAMTransfer_BeginFrame(); run(); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==2 && latest(1)==2 && S68K_RW(base+0x100)==4);
        opcode(0x200,0x48d0); opcode(0x202,7); // MOVEM.L D0-D2,(A0)
        opcode(0x204,0x4cd0); opcode(0x206,7); // MOVEM.L (A0),D0-D2
        sub68k_context.dreg[0]=0x12345678; sub68k_context.dreg[1]=0x90abcdef; sub68k_context.dreg[2]=0x87654321;
        WordRAMTransfer_BeginFrame(); run();
        sub68k_context.dreg[0]=sub68k_context.dreg[1]=sub68k_context.dreg[2]=0;
        run(0x204); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==12 && latest(1)==12);
        CHECK(sub68k_context.dreg[0]==0x12345678 && sub68k_context.dreg[1]==0x90abcdef && sub68k_context.dreg[2]==0x87654321);
        // Execution from Word RAM does not count instruction/extension fetches.
        opcode(base+0x300,0x60fe);
        WordRAMTransfer_BeginFrame(); run(base+0x300,200); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==0);
        // PC-relative operand reads still count as data.
        opcode(base+0x300,0x303a); opcode(base+0x302,0x1e); opcode(base+0x320,0x7654);
        WordRAMTransfer_BeginFrame(); run(base+0x300); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==2 && latest(1)==0 && (sub68k_context.dreg[0]&0xffff)==0x7654);
    }
    Ram_Word_State=1; MS68K_Set_Word_Ram();
    opcode(0x200,0x2010); opcode(0x202,0x2280);
    sub68k_context.areg[0]=0x0bfffe; sub68k_context.areg[1]=0x0bfffe;
    opcode(0x0bfffe,0xbeef);
    WordRAMTransfer_BeginFrame(); run(); run(0x202); WordRAMTransfer_EndFrame(60);
    CHECK(latest(0)==2 && latest(1)==2); // Second word is unmapped in 2M mode.
    sub68k_context.areg[0]=0x01080100; sub68k_context.areg[1]=0x01080200;
    WordRAMTransfer_BeginFrame(); run(); run(0x202); WordRAMTransfer_EndFrame(60);
    CHECK(latest(0)==4 && latest(1)==4); // CPU's 24-bit address mask.
    for (unsigned i=0;i<3;i++) {
        const unsigned addresses[]={0x6000,0x0c0000,0x0e0000};
        sub68k_context.areg[0]=sub68k_context.areg[1]=addresses[i];
        WordRAMTransfer_BeginFrame(); run(); run(0x202); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==0);
    }
    for (unsigned mode=2;mode<4;mode++) {
        Ram_Word_State=mode; S68K_Mem_PM=0; MS68K_Set_Word_Ram();
        opcode(0x200,0x3010); opcode(0x202,0x3280);
        sub68k_context.areg[0]=0x080100; sub68k_context.areg[1]=0x080200;
        opcode(0x080100,0x0a05);
        WordRAMTransfer_BeginFrame(); run(); run(0x202); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==2 && latest(1)==2 && S68K_RW(0x080200)==0x0a05);
        // Underwrite rejects nonzero destination pixels but still uses the bus.
        S68K_Mem_PM=8; sub68k_context.dreg[0]=0x0f0f;
        WordRAMTransfer_BeginFrame(); run(0x202); WordRAMTransfer_EndFrame(60);
        CHECK(latest(0)==0 && latest(1)==2 && S68K_RW(0x080200)==0x0a05);
    }
    puts("PASS: real byte/word/long/MOVEM/RMW and PC-relative accesses; 1M/2M banks, dot-image, boundaries, instruction fetch excluded");
    return 0;
}

static int isolation_and_history_tests(void) {
    Ram_Word_State=1; S68K_Mem_PM=0; MS68K_Set_Word_Ram();
    WordRAMTransfer_Reset();
    S68K_Data_RB(0x80100); S68K_Data_WW(0x80200,7); WordRAMTransfer_EndFrame(60);
    CHECK(!WordRAMTransfer_GetHistory(0,values));
    WordRAMTransfer_BeginFrame();
    // Debugger/raw helpers and main-CPU data accesses must not be counted.
    S68K_RB(0x80100); S68K_RW(0x80100); S68K_WB(0x80200,1); S68K_WW(0x80200,2);
    Init_Memory_M68K(2); M68K_RB(0x200100); M68K_RW(0x200100); M68K_WW(0x200200,3);
    // CDC DMA really writes Word RAM, but is not sub-CPU traffic.
    CDC.RS0=0x700; CDC.DMA_Adr=0; CDC.DBC.N=4; CDC.DAC.N=0; CDC.IFCTRL=0;
    CDC.Buffer[0]=0x12; CDC.Buffer[1]=0x34; SCD.Status_CDC=8;
    Update_CDC_TRansfert();
    CHECK(S68K_RW(0x80000)==0x1234);
    WordRAMTransfer_EndFrame(60); CHECK(latest(0)==0 && latest(1)==0);
    for (unsigned i=0;i<300;i++) sample(i,300-i);
    CHECK(WordRAMTransfer_GetHistory(0,values)==256 && values[0]==44 && values[255]==299);
    CHECK(WordRAMTransfer_GetHistory(1,values)==256 && values[0]==256 && values[255]==1);
    for (unsigned fps=50;fps<=60;fps+=10) {
        WordRAMTransfer_Reset();
        for (unsigned i=0;i<fps;i++) sample(1024,2048,fps);
        CHECK(WordRAMTransfer_GetBytesPerSecond(0)==fps*1024 && WordRAMTransfer_GetBytesPerSecond(1)==fps*2048);
        for (unsigned i=0;i<fps;i++) sample(0,0,fps);
        CHECK(!WordRAMTransfer_GetBytesPerSecond(0) && !WordRAMTransfer_GetBytesPerSecond(1));
    }
    sample(2,4,50); CHECK(WordRAMTransfer_GetHistory(0,values)==1);
    WordRAMTransfer_EndFrame(50); CHECK(WordRAMTransfer_GetHistory(0,values)==1);
    S68K_Reset_CPU(); CHECK(!WordRAMTransfer_GetHistory(0,values));
    sample(2,4); S68K_Reset(); CHECK(!WordRAMTransfer_GetHistory(1,values));
    puts("PASS: debugger/main CPU/DMA excluded; independent histories and rates, PAL/NTSC, idle decay and CPU reset");
    return 0;
}

static int panel_tests(void) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"wrt",0,config));
    SegaCD_Started=1; CD_32X_Active=0; Paused=0; Ram_Word_State=1;
    WordRAMTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    wordramtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_WORD_RAM_TRANSFER-1]; CHECK(panel);
    wordramtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    wordramtransfer_update();
    GetDlgItemText(panel,IDC_WORD_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_WORD_RAM_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_WORD_RAM_READ_GRAPH,IDC_WORD_RAM_WRITE_GRAPH};
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
    for (unsigned i=0;i<10;i++) wordramtransfer_update();
    CHECK(WordRAMTransfer_GetHistory(0,values)==256 && latest(0)==1024 && latest(1)==8192);
    GetDlgItemText(panel,IDC_WORD_RAM_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"bank 1") && strstr(text,"Paused"));
    SegaCD_Started=0; wordramtransfer_update();
    GetDlgItemText(panel,IDC_WORD_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; wordramtransfer_update();
    GetDlgItemText(panel,IDC_WORD_RAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
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
    OpenWindow_KMod(DMODE_WORD_RAM_TRANSFER); GetWindowRect(panel,&saved); wordramtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !WordRAMTransfer_GetHistory(0,values));
    wordramtransfer_destroy(); wordramtransfer_create(GetModuleHandle(NULL),NULL); wordramtransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_WORD_RAM_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    wordramtransfer_save_window(config); wordramtransfer_destroy();
    wordramtransfer_create(GetModuleHandle(NULL),NULL); wordramtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_WORD_RAM_TRANSFER-1]); wordramtransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    M68K_Init(); S68K_Init(); Init_Tab();
    return instruction_tests() || isolation_and_history_tests() || panel_tests();
}
