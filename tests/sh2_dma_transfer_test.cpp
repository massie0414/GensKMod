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
#include "../src/Gens/sh2_dma_transfer.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/sh2_dma_transfer_panel.h"
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned long values[SH2_DMA_TRANSFER_HISTORY];
static unsigned long latest(int cpu,int write) {
    unsigned n=SH2DMATransfer_GetHistory(cpu,write,values);return n?values[n-1]:~0UL;
}
static void configure(SH2_CONTEXT *cpu,unsigned channel,unsigned src,unsigned dst,unsigned count,unsigned control) {
    unsigned reg=0xffffff80+channel*16;
    SH2_Write_Long(cpu,reg+12,0);
    SH2_Write_Long(cpu,reg,src);SH2_Write_Long(cpu,reg+4,dst);SH2_Write_Long(cpu,reg+8,count);
    SH2_Write_Long(cpu,0xffffffb0,1);SH2_Write_Long(cpu,reg+12,control);
}
static int internal_tests(void) {
    _32X_Started=1;CD_32X_Active=Genesis_Started=SegaCD_Started=0;
    for(int owner=0;owner<2;owner++) for(unsigned channel=0;channel<2;channel++)
    for(unsigned size=0;size<4;size++) for(unsigned sm=0;sm<3;sm++) for(unsigned dm=0;dm<3;dm++) {
        SH2_CONTEXT *cpu=owner?&S_SH2:&M_SH2;
        unsigned width=size==3?4:1<<size,units=size==3?8:2;
        unsigned src=0x06001000+(sm==2?units*width:0),dst=0x06002000+(dm==2?units*width:0);
        unsigned char expected[64];memset(expected,0xa5,64);
        for(unsigned i=0;i<64;i++) { SH2_Write_Byte(cpu,0x06001000+i,(unsigned char)(i+17));SH2_Write_Byte(cpu,0x06002000+i,0xa5); }
        for(unsigned i=0;i<units;i++) {
            unsigned si=(sm==2?units-i-1:sm==1?i:0)*width,di=(dm==2?units-i-1:dm==1?i:0)*width;
            for(unsigned b=0;b<width;b++)expected[di+b]=(unsigned char)(si+b+17);
        }
        SH2DMATransfer_Reset();SH2DMATransfer_BeginFrame();
        configure(cpu,channel,src,dst,2,0x201|(size<<10)|(sm<<12)|(dm<<14));
        SH2DMATransfer_EndFrame(60);
        CHECK(latest(owner,0)==units*width && latest(owner,1)==units*width);
        CHECK(!latest(1-owner,0) && !latest(1-owner,1));
        CHECK(SH2_Read_Long(cpu,0xffffff88+channel*16)==0 && (SH2_Read_Long(cpu,0xffffff8c+channel*16)&2));
        for(unsigned i=0;i<64;i++)CHECK(SH2_Read_Byte(cpu,0x06002000+i)==expected[i]);
    }
    // Both channels accumulate for one CPU, cache-through aliases still transfer once.
    SH2DMATransfer_BeginFrame();configure(&M_SH2,0,0x26001000,0x26002000,2,0x5601);
    configure(&M_SH2,1,0x06001000,0x06002010,3,0x5a01);SH2DMATransfer_EndFrame(60);
    CHECK(latest(0,0)==16 && latest(0,1)==16);
    puts("PASS: both CPUs/channels, byte/word/long/16-byte DMA, all fixed/increment/decrement combinations, payload and channel totals");
    return 0;
}
static SH2_CONTEXT *request_cpu;
static unsigned request_channel,stop_after,reads,writes;
static unsigned char FASTCALL request_read_byte(unsigned long) { ++reads;return 0x34; }
static unsigned short FASTCALL request_read_word(unsigned long) { ++reads;return 0x1234; }
static unsigned long FASTCALL request_read_long(unsigned long) { ++reads;return 0x12345678; }
static void stop_request(void) {
    if(++writes==stop_after) { if(request_channel)request_cpu->DREQ1=0;else request_cpu->DREQ0=0; }
}
static void FASTCALL request_write_byte(unsigned long,unsigned char) { stop_request(); }
static void FASTCALL request_write_word(unsigned long,unsigned short) { stop_request(); }
static void FASTCALL request_write_long(unsigned long,unsigned long) { stop_request(); }
static int external_tests(void) {
    for(int owner=0;owner<2;owner++) {
        SH2_CONTEXT *cpu=owner?&S_SH2:&M_SH2;request_cpu=cpu;
        SH2_Add_ReadB(cpu,0x10,0x10,request_read_byte);SH2_Add_ReadW(cpu,0x10,0x10,request_read_word);SH2_Add_ReadL(cpu,0x10,0x10,request_read_long);
        SH2_Add_WriteB(cpu,0x11,0x11,request_write_byte);SH2_Add_WriteW(cpu,0x11,0x11,request_write_word);SH2_Add_WriteL(cpu,0x11,0x11,request_write_long);
        for(unsigned ch=0;ch<2;ch++) for(unsigned size=0;size<4;size++) for(unsigned sm=0;sm<3;sm++) for(unsigned dm=0;dm<3;dm++) {
            request_channel=ch;unsigned width=size==3?4:1<<size;
            cpu->DREQ0=cpu->DREQ1=0;reads=writes=0;stop_after=3;
            SH2DMATransfer_Reset();SH2DMATransfer_BeginFrame();
            configure(cpu,ch,0x10001000,0x11002000,5,1|(size<<10)|(sm<<12)|(dm<<14));
            SH2DMATransfer_EndFrame(60);CHECK(!latest(owner,0) && !latest(owner,1) && !reads);
            SH2DMATransfer_BeginFrame();if(ch)SH2_DMA1_Request(cpu,1);else SH2_DMA0_Request(cpu,1);SH2DMATransfer_EndFrame(60);
            CHECK(reads==3 && writes==3 && latest(owner,0)==3*width && latest(owner,1)==3*width);
            CHECK(SH2_Read_Long(cpu,0xffffff88+ch*16)==(size==3?17:2));
            if(size!=3) {
                reads=writes=0;stop_after=2;
                SH2DMATransfer_BeginFrame();if(ch)SH2_DMA1_Request(cpu,1);else SH2_DMA0_Request(cpu,1);SH2DMATransfer_EndFrame(60);
                CHECK(reads==2 && writes==2 && latest(owner,0)==2*width && (SH2_Read_Long(cpu,0xffffff8c+ch*16)&2));
            }
        }
        cpu->DREQ0=cpu->DREQ1=0;SH2_Write_Long(cpu,0xffffff8c,0);SH2_Write_Long(cpu,0xffffff9c,0);
    }
    puts("PASS: all external DMA paths, pending requests excluded, partial transfer and resumption count only performed operations");
    return 0;
}
static int peripheral_tests(void) {
    for(int owner=0;owner<2;owner++) {
        SH2_CONTEXT *cpu=owner?&S_SH2:&M_SH2;
        SH2_Reset(cpu,0);SH2_DMA1_Request(cpu,0);PWM_Init();PWM_Enable=1;_32X_MINT=_32X_SINT=0;
        for(unsigned i=0;i<4;i++)SH2_Write_Long(cpu,0x06010000+i*4,0x03000400+i*0x10001);
        configure(cpu,1,0x06010000,0x20004034,4,0x18e1);
        PWM_Mode=0x185;PWM_Set_Cycle(32);PWM_Set_Int(1);
        SH2DMATransfer_Reset();
        for(unsigned i=0;i<4;i++) {
            SH2DMATransfer_BeginFrame();PWM_Update_Timer(i*31);SH2DMATransfer_EndFrame(60);
            if(latest(owner,0)!=4 || latest(owner,1)!=4 || cpu->TCR1!=3-i) printf("PWM owner=%d i=%u bytes=%lu/%lu remaining=%lu request=%u\n",owner,i,latest(owner,0),latest(owner,1),cpu->TCR1,cpu->DREQ1);
            CHECK(latest(owner,0)==4 && latest(owner,1)==4 && cpu->TCR1==3-i);
        }
        CHECK(PWM_FIFO_L[3]==0x303 && PWM_FIFO_R[3]==0x403);
    }
    PWM_Enable=0;
    // The real 32X FIFO deasserts DREQ0 after four words.
    SH2_Reset(&M_SH2,0);_32X_DREQ_ST=4;_32X_DREQ_LEN=4;_32X_FIFO_Read=0;_32X_FIFO_Block=0;
    for(unsigned i=0;i<4;i++)_32X_FIFO_B[i]=(unsigned short)(0x1230+i);
    configure(&M_SH2,0,0x20004012,0x06003000,4,0x44e1);
    SH2DMATransfer_Reset();SH2DMATransfer_BeginFrame();SH2_DMA0_Request(&M_SH2,1);SH2DMATransfer_EndFrame(60);
    CHECK(latest(0,0)==8 && latest(0,1)==8 && M_SH2.TCR0==0);
    for(unsigned i=0;i<4;i++)CHECK(SH2_Read_Word(&M_SH2,0x06003000+i*2)==0x1230+i);
    puts("PASS: real PWM paced DMA on both SH2s and FIFO-requested master DMA");return 0;
}
static void sample(unsigned master,unsigned slave,unsigned fps=60) {
    SH2DMATransfer_BeginFrame();
    if(master)configure(&M_SH2,0,0x06001000,0x06002000,master,0x201);
    if(slave)configure(&S_SH2,1,0x06001000,0x06002000,slave,0x201);
    SH2DMATransfer_EndFrame(fps);
}
static int history_tests(void) {
    SH2DMATransfer_Reset();
    SH2_Read_Long(&M_SH2,0x06001000);SH2_Write_Long(&S_SH2,0x06002000,0);
    configure(&M_SH2,0,0x06001000,0x06002000,4,0x201);SH2DMATransfer_EndFrame(60);
    CHECK(!SH2DMATransfer_GetHistory(0,0,values));
    SH2DMATransfer_BeginFrame();
    SH2_Read_Long(&M_SH2,0x06001000);SH2_Write_Long(&S_SH2,0x06002000,0);
    configure(&M_SH2,0,0x06001000,0x06002000,4,0x200); // channel disabled
    SH2_Write_Long(&M_SH2,0xffffffb0,0);SH2_Write_Long(&M_SH2,0xffffff8c,0x201); // controller disabled
    SH2DMATransfer_EndFrame(60);CHECK(!latest(0,0) && !latest(0,1) && !latest(1,0) && !latest(1,1));
    SH2_Write_Long(&M_SH2,0xffffff8c,0);
    for(unsigned i=0;i<300;i++)sample(i,300-i);
    CHECK(SH2DMATransfer_GetHistory(0,0,values)==256 && values[0]==44 && values[255]==299);
    CHECK(SH2DMATransfer_GetHistory(1,1,values)==256 && values[0]==256 && values[255]==1);
    for(unsigned fps=50;fps<=60;fps+=10) {
        SH2DMATransfer_Reset();for(unsigned i=0;i<fps;i++)sample(1024,2048,fps);
        CHECK(SH2DMATransfer_GetBytesPerSecond(0,0)==fps*1024 && SH2DMATransfer_GetBytesPerSecond(1,1)==fps*2048);
        for(unsigned i=0;i<fps;i++)sample(0,0,fps);
        CHECK(!SH2DMATransfer_GetBytesPerSecond(0,0) && !SH2DMATransfer_GetBytesPerSecond(1,1));
    }
    sample(2,4,50);SH2DMATransfer_EndFrame(50);CHECK(SH2DMATransfer_GetHistory(0,0,values)==1);
    SH2_Reset(&M_SH2,1);CHECK(!SH2DMATransfer_GetHistory(0,0,values) && SH2DMATransfer_GetHistory(1,0,values)==1);
    SH2_Reset(&S_SH2,0);CHECK(!SH2DMATransfer_GetHistory(1,0,values));
    _32X_Started=0;Genesis_Started=1;sample(2,4);CHECK(!SH2DMATransfer_GetHistory(0,0,values));_32X_Started=1;Genesis_Started=0;
    puts("PASS: histories/rates, PAL/NTSC, idle decay, outside-frame exclusion and independent CPU resets");return 0;
}
static int panel_tests(int kind) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"vrt",0,config));
    _32X_Started=1; Genesis_Started=SegaCD_Started=CD_32X_Active=0; Paused=0;
    SH2DMATransfer_Reset(); for (unsigned i=0;i<256;i++) sample(1024,1024);
    sh2dmatransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[(DMODE_MASTER_SH2_DMA_TRANSFER+kind)-1]; CHECK(panel);
    sh2dmatransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    sh2dmatransfer_update();
    GetDlgItemText(panel,IDC_SH2_DMA_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_SH2_DMA_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_SH2_DMA_READ_GRAPH,IDC_SH2_DMA_WRITE_GRAPH};
    for (unsigned i=0;i<2;i++) {
        item.hwndItem=GetDlgItem(panel,graphs[i]); CHECK(item.hwndItem);
        SendMessage(panel,WM_DRAWITEM,graphs[i],(LPARAM)&item);
        CHECK(GetPixel(dc,100,40)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,145)==RGB(0,0,255));
        CHECK(GetPixel(dc,100,110)==GetSysColor(COLOR_WINDOW));
    }
    // A write burst raises the common axis for both graphs.
    sample(8192,8192); item.hwndItem=GetDlgItem(panel,graphs[0]);
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item);
    CHECK(GetPixel(dc,100,135)==GetSysColor(COLOR_WINDOW));
    Paused=1;
    for (unsigned i=0;i<10;i++) sh2dmatransfer_update();
    CHECK(SH2DMATransfer_GetHistory(kind,0,values)==256 && latest(kind,0)==8192 && latest(kind,1)==8192);
    GetDlgItemText(panel,IDC_SH2_DMA_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"Paused"));
    _32X_Started=0; sh2dmatransfer_update();
    GetDlgItemText(panel,IDC_SH2_DMA_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; sh2dmatransfer_update();
    GetDlgItemText(panel,IDC_SH2_DMA_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"67.0"));
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
    OpenWindow_KMod((DMODE_MASTER_SH2_DMA_TRANSFER+kind)); GetWindowRect(panel,&saved); sh2dmatransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !SH2DMATransfer_GetHistory(kind,0,values));
    sh2dmatransfer_destroy(); sh2dmatransfer_create(GetModuleHandle(NULL),NULL); sh2dmatransfer_restore_window(config);
    panel=HandleWindow_KMod[(DMODE_MASTER_SH2_DMA_TRANSFER+kind)-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    for(int other=0;other<2;other++) if(other!=kind) CHECK(!IsWindowVisible(HandleWindow_KMod[DMODE_MASTER_SH2_DMA_TRANSFER+other-1]));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    sh2dmatransfer_save_window(config); sh2dmatransfer_destroy();
    sh2dmatransfer_create(GetModuleHandle(NULL),NULL); sh2dmatransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[(DMODE_MASTER_SH2_DMA_TRANSFER+kind)-1]); sh2dmatransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    M68K_Init();S68K_Init();Init_Tab();MSH2_Init();SSH2_Init();
    return internal_tests() || external_tests() || peripheral_tests() || history_tests() || panel_tests(0) || panel_tests(1);
}
