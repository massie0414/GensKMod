// Real SH2 DMAC register paths, external requests and dialog resources; no game/BIOS required.
#include <windows.h>
#include <stdio.h>
#include <string.h>
extern "C" {
#include "../src/Gens/gens.h"
#include "../src/Gens/kmod/common.h"
}
#include "../src/Gens/SH2.h"
#include "../src/Gens/Cpu_SH2.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/Mem_SH2.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/pwm.h"
#include "../src/Gens/sdram_transfer.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/sdram_transfer_panel.h"
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned long values[SDRAM_TRANSFER_HISTORY];
static unsigned long latest(int write) {
    unsigned n=SDRAMTransfer_GetHistory(write,values); return n?values[n-1]:~0UL;
}
static void configure(SH2_CONTEXT *cpu,unsigned src,unsigned dst,unsigned count,unsigned control=0x201) {
    SH2_Write_Long(cpu,0xffffff8c,0); SH2_Write_Long(cpu,0xffffff80,src);
    SH2_Write_Long(cpu,0xffffff84,dst); SH2_Write_Long(cpu,0xffffff88,count);
    SH2_Write_Long(cpu,0xffffffb0,1); SH2_Write_Long(cpu,0xffffff8c,control);
}
static void sample(unsigned reads,unsigned writes,unsigned fps=60) {
    SDRAMTransfer_BeginFrame();
    if(reads)configure(&M_SH2,0x06001000,0xc0000000,reads);
    if(writes)configure(&S_SH2,0x02001000,0x06002000,writes);
    SDRAMTransfer_EndFrame(fps);
}
static void put(unsigned offset,unsigned op) { _32X_Ram[offset]=(unsigned char)(op>>8);_32X_Ram[offset+1]=(unsigned char)op; }
static void prepare(SH2_CONTEXT *cpu) {
    SH2_Reset(cpu,0); cpu->Status=0; SH2_Set_SR(cpu,0xf0);
    SH2_Clear_Odo(cpu); SH2_Set_PC(cpu,0x06000004);
}
static int instruction_tests(void) {
    _32X_Started=1; Genesis_Started=SegaCD_Started=CD_32X_Active=0;
    for(int owner=0;owner<2;owner++) for(unsigned alias=0;alias<4;alias++) for(unsigned size=0;size<3;size++) {
        SH2_CONTEXT *cpu=owner?&S_SH2:&M_SH2; prepare(cpu);
        unsigned base=(alias&1?0x26000000:0x06000000)+(alias&2?0x00fc0000:0);
        cpu->R[1]=base+0x1000; cpu->R[2]=base+0x2000;
        SH2_Write_Long(cpu,cpu->R[1],0x12345678);SH2_Write_Long(cpu,cpu->R[2],0);
        put(0,0x6010+size);put(2,0x2200+size);put(4,0x001b); // MOV load/store; SLEEP
        SDRAMTransfer_BeginFrame();SH2_Exec(cpu,100);SDRAMTransfer_EndFrame(60);
        CHECK(latest(0)==(1u<<size) && latest(1)==(1u<<size));
        for(unsigned b=0;b<(1u<<size);b++)CHECK(_32X_Ram[0x1000+b]==_32X_Ram[0x2000+b]);
        CHECK(!(cpu->Status&1));
    }
    // Both CPUs contribute to the same frame; postincrement/predecrement and stack stores.
    prepare(&M_SH2);prepare(&S_SH2);
    M_SH2.R[1]=S_SH2.R[1]=0x26001000;M_SH2.R[2]=0x06002004;S_SH2.R[2]=0x26002008;
    put(0,0x6016);put(2,0x2206);put(4,0x001b);
    SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SH2_Exec(&S_SH2,100);SDRAMTransfer_EndFrame(60);
    CHECK(latest(0)==8 && latest(1)==8 && M_SH2.R[1]==0x26001004 && S_SH2.R[2]==0x26002004);
    // RMW byte instructions count their read and write, with unchanged T flag semantics.
    for(int owner=0;owner<2;owner++) {
        SH2_CONTEXT *cpu=owner?&S_SH2:&M_SH2;prepare(cpu);cpu->R[1]=0x0603ffff;_32X_Ram[0x3ffff]=0;
        put(0,0x411b);put(2,0x001b); // TAS.B @R1
        SDRAMTransfer_BeginFrame();SH2_Exec(cpu,100);SDRAMTransfer_EndFrame(60);
        CHECK(latest(0)==1 && latest(1)==1 && _32X_Ram[0x3ffff]==0x80 && (SH2_Get_SR(cpu)&1));
    }
    // PC-relative literals are data reads; the surrounding instruction fetch is excluded.
    prepare(&M_SH2);put(0,0x9004);put(2,0xd103);put(4,0x001b);
    put(12,0x1234);SH2_Write_Long(&M_SH2,0x06000010,0x12345678);
    SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SDRAMTransfer_EndFrame(60);
    CHECK(latest(0)==6 && !latest(1) && M_SH2.R[0]==0x1234 && M_SH2.R[1]==0x12345678);
    for(unsigned size=0;size<2;size++) {
        prepare(&S_SH2);put(0,0xa00e);put(2,size?0xd003:0x9004);put(0x20,0x001b);
        SDRAMTransfer_BeginFrame();SH2_Exec(&S_SH2,100);SDRAMTransfer_EndFrame(60);
        CHECK(latest(0)==(size?4:2) && !latest(1)); // literal in a branch delay slot
    }
    // Identical PC-relative loads from ROM do not count as SDRAM.
    prepare(&M_SH2);unsigned char old_rom[32];memcpy(old_rom,_32X_Rom,32);
    memcpy(_32X_Rom,_32X_Ram,32);_32X_Rom[0]=0x90;_32X_Rom[1]=4;_32X_Rom[2]=0;_32X_Rom[3]=0x1b;
    SH2_Set_PC(&M_SH2,0x02000004);SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SDRAMTransfer_EndFrame(60);
    CHECK(!latest(0) && !latest(1));memcpy(_32X_Rom,old_rom,32);
    // Aligned accesses at the SDRAM end follow the existing native handler alignment.
    prepare(&M_SH2);M_SH2.R[1]=M_SH2.R[2]=0x26ffffff;
    put(0,0x6012);put(2,0x2202);put(4,0x001b);
    SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SDRAMTransfer_EndFrame(60);
    CHECK(latest(0)==4 && latest(1)==4);
    // External interrupt entry occurs before SH2_RUNNING on the first slice.
    prepare(&M_SH2);M_SH2.R[15]=0x06020000;SH2_Set_VBR(&M_SH2,0x06001000);SH2_Set_SR(&M_SH2,0);
    SH2_Write_Long(&M_SH2,0x06001000+68*4,0x06000080);put(0x80,0x001b);SH2_Interrupt(&M_SH2,8);
    SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SDRAMTransfer_EndFrame(60);
    CHECK(latest(0)==4 && latest(1)==8 && M_SH2.R[15]==0x0601fff8);
    puts("PASS: both SH2 CPUs, byte/word/long, mirrored aliases, RMW, PC-relative data, address updates and interrupt stack/vector accesses");
    return 0;
}
static int isolation_and_history_tests(void) {
    prepare(&M_SH2);prepare(&S_SH2);put(0,0xaffe);put(2,9); // branch and delay slot fetch only
    SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SH2_Exec(&S_SH2,100);
    SH2_Read_Byte(&M_SH2,0x06001000);SH2_Read_Word(&S_SH2,0x26001000);SH2_Read_Long(&M_SH2,0x06001000);
    SH2_Write_Byte(&M_SH2,0x06002000,1);SH2_Write_Word(&S_SH2,0x26002000,2);SH2_Write_Long(&M_SH2,0x06002000,3);
    SDRAMTransfer_EndFrame(60);CHECK(!latest(0) && !latest(1));
    // Real data traffic to ROM, framebuffer, cache and on-chip registers must not count as SDRAM.
    const unsigned other[]={0x02001000,0x04001000,0xc0000000,0xffffff80};
    for(unsigned i=0;i<4;i++) {
        prepare(&M_SH2);M_SH2.R[1]=other[i];M_SH2.R[2]=other[i];
        put(0,0x6012);put(2,0x2202);put(4,0x001b);
        SDRAMTransfer_BeginFrame();SH2_Exec(&M_SH2,100);SDRAMTransfer_EndFrame(60);
        CHECK(!latest(0) && !latest(1));
    }
    // DMA reads and writes have independent RAM endpoints, and aggregate both CPUs.
    SDRAMTransfer_BeginFrame();configure(&M_SH2,0x26001000,0x06002000,3,0x5a01);
    configure(&S_SH2,0x02001000,0x26003000,2,0x5a01);SDRAMTransfer_EndFrame(60);
    CHECK(latest(0)==12 && latest(1)==20);
    // Pending external requests and transfers outside a frame are excluded.
    prepare(&M_SH2);M_SH2.DREQ0=0;
    configure(&M_SH2,0x06001000,0xc0000000,3,0x0001);
    SDRAMTransfer_BeginFrame();SDRAMTransfer_EndFrame(60);CHECK(!latest(0) && !latest(1));
    configure(&M_SH2,0x06001000,0x06002000,3); // Outside a frame is not recorded.
    SDRAMTransfer_EndFrame(60);CHECK(!latest(0));SH2_DMA0_Request(&M_SH2,0);
    for(unsigned i=0;i<300;i++)sample(i,300-i);
    CHECK(SDRAMTransfer_GetHistory(0,values)==256 && values[0]==44 && values[255]==299);
    CHECK(SDRAMTransfer_GetHistory(1,values)==256 && values[0]==256 && values[255]==1);
    for(unsigned fps=50;fps<=60;fps+=10) {
        SDRAMTransfer_Reset();for(unsigned i=0;i<fps;i++)sample(1024,2048,fps);
        CHECK(SDRAMTransfer_GetBytesPerSecond(0)==fps*1024 && SDRAMTransfer_GetBytesPerSecond(1)==fps*2048);
        for(unsigned i=0;i<fps;i++)sample(0,0,fps);
        CHECK(!SDRAMTransfer_GetBytesPerSecond(0) && !SDRAMTransfer_GetBytesPerSecond(1));
    }
    sample(2,4,50);SDRAMTransfer_EndFrame(50);CHECK(SDRAMTransfer_GetHistory(0,values)==1);
    SH2_Reset(&M_SH2,1);CHECK(!SDRAMTransfer_GetHistory(0,values));
    sample(2,4);SH2_Reset(&S_SH2,0);CHECK(!SDRAMTransfer_GetHistory(1,values));
    _32X_Started=0;Genesis_Started=1;sample(2,4);CHECK(!SDRAMTransfer_GetHistory(0,values));
    Genesis_Started=0;SegaCD_Started=1;sample(2,4);CHECK(!SDRAMTransfer_GetHistory(0,values));
    SegaCD_Started=0;CD_32X_Active=1;sample(2,4);CHECK(latest(0)==2 && latest(1)==4);CD_32X_Active=0;_32X_Started=1;
    puts("PASS: instruction fetch/debugger/other memory excluded, DMA directions, histories, PAL/NTSC, idle decay, resets and inactive sessions");return 0;
}
static int panel_tests(void) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"vrt",0,config));
    _32X_Started=1; Genesis_Started=SegaCD_Started=CD_32X_Active=0; Paused=0;
    SDRAMTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    sdramtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_SDRAM_TRANSFER-1]; CHECK(panel);
    sdramtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    sdramtransfer_update();
    GetDlgItemText(panel,IDC_SDRAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_SDRAM_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_SDRAM_READ_GRAPH,IDC_SDRAM_WRITE_GRAPH};
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
    for (unsigned i=0;i<10;i++) sdramtransfer_update();
    CHECK(SDRAMTransfer_GetHistory(0,values)==256 && latest(0)==1024 && latest(1)==8192);
    GetDlgItemText(panel,IDC_SDRAM_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"256 KiB") && strstr(text,"Paused"));
    _32X_Started=0; sdramtransfer_update();
    GetDlgItemText(panel,IDC_SDRAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; sdramtransfer_update();
    GetDlgItemText(panel,IDC_SDRAM_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
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
    OpenWindow_KMod(DMODE_SDRAM_TRANSFER); GetWindowRect(panel,&saved); sdramtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !SDRAMTransfer_GetHistory(0,values));
    sdramtransfer_destroy(); sdramtransfer_create(GetModuleHandle(NULL),NULL); sdramtransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_SDRAM_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    sdramtransfer_save_window(config); sdramtransfer_destroy();
    sdramtransfer_create(GetModuleHandle(NULL),NULL); sdramtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_SDRAM_TRANSFER-1]); sdramtransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    setbuf(stdout,NULL);M68K_Init();S68K_Init();Init_Tab();MSH2_Init();SSH2_Init();
    return instruction_tests() || isolation_and_history_tests() || panel_tests();
}
