// Actual PWM enqueue/dequeue, CPU/DMA paths and dialog resources; no game/BIOS required.
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
#include "../src/Gens/pwm_fifo_transfer.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/pwm_fifo_transfer_panel.h"
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned long values[PWM_FIFO_TRANSFER_HISTORY];
static unsigned tick;
static unsigned long latest(int write) { unsigned n=PWMFIFOTransfer_GetHistory(write,values);return n?values[n-1]:~0UL; }
static SH2_CONTEXT *cpu(int owner) { return owner==1?&M_SH2:&S_SH2; }
static unsigned port(int owner,unsigned offset=0) { return (owner?0x20004030:0xa15130)+offset; }
static void bus_write(int owner,unsigned address,unsigned width,unsigned value) {
    if(owner) {
        if(width==1)SH2_Write_Byte(cpu(owner),address,(unsigned char)value);
        else if(width==2)SH2_Write_Word(cpu(owner),address,(unsigned short)value);
        else SH2_Write_Long(cpu(owner),address,value);
    } else {
        if(width==1)M68K_WB(address,(unsigned char)value);
        else if(width==2)M68K_WW(address,(unsigned short)value);
        else { M68K_WW(address,(unsigned short)(value>>16));M68K_WW(address+2,(unsigned short)value); }
    }
}
static void reset_pwm(void) {
    for(int i=1;i<=2;i++) {
        SH2_Write_Long(cpu(i),0xffffff8c,0);SH2_Write_Long(cpu(i),0xffffff9c,0);
        cpu(i)->DREQ0=cpu(i)->DREQ1=0;
    }
    PWM_Cycles=0;PWM_Init();PWM_Enable=1;PWM_Mode=5;PWM_Set_Cycle(32);PWM_Set_Int(1);_32X_MINT=_32X_SINT=0;tick=0;
}
static void step(void) { PWM_Update_Timer(tick);tick+=31; }
static int enqueue_tests(void) {
    _32X_Started=1;Genesis_Started=SegaCD_Started=CD_32X_Active=0;Init_Memory_M68K(1);
    for(int owner=0;owner<3;owner++)for(unsigned channel=0;channel<3;channel++)for(unsigned alias=0;alias<2;alias++) {
        reset_pwm();unsigned address=port(owner,4+channel*2)+(alias?(owner?-0x20000000:0x40):0);
        unsigned bytes=channel==2?4:2;
        PWMFIFOTransfer_BeginFrame();bus_write(owner,address,1,0x12);PWMFIFOTransfer_EndFrame(60);
        CHECK(!latest(0) && !latest(1) && !PWM_WP_L && !PWM_WP_R);
        for(unsigned i=0;i<3;i++) {
            PWMFIFOTransfer_BeginFrame();bus_write(owner,address+1,1,0x34+i);PWMFIFOTransfer_EndFrame(60);
            CHECK(!latest(0) && latest(1)==bytes);
            if(channel!=1)CHECK(PWM_WP_L==i+1 && PWM_FIFO_L[i]==0x1234+i);
            if(channel!=0)CHECK(PWM_WP_R==i+1 && PWM_FIFO_R[i]==0x1234+i);
        }
        PWMFIFOTransfer_BeginFrame();bus_write(owner,address,2,0x9876);PWMFIFOTransfer_EndFrame(60);CHECK(!latest(1));
        PWMFIFOTransfer_BeginFrame();step();bus_write(owner,address,2,0);PWMFIFOTransfer_EndFrame(60);
        CHECK(latest(0)==bytes && latest(1)==bytes); // zero sample accepted after one dequeue
        if(channel!=1)CHECK(PWM_WP_L==0 && PWM_FIFO_L[3]==0 && PWM_Out_L==0x1234);
        if(channel!=0)CHECK(PWM_WP_R==0 && PWM_FIFO_R[3]==0 && PWM_Out_R==0x1234);
        for(unsigned i=0;i<3;i++) {
            PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);
            CHECK(latest(0)==bytes && !latest(1));
            unsigned expected=i<2?0x1235+i:0;
            if(channel!=1)CHECK(PWM_Out_L==expected);
            if(channel!=0)CHECK(PWM_Out_R==expected);
        }
        PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);CHECK(!latest(0));
        // The FIFO does not consume samples when software polls its full/empty status.
        PWMFIFOTransfer_BeginFrame();
        for(unsigned i=0;i<20;i++)if(owner) { SH2_Read_Byte(cpu(owner),address);SH2_Read_Word(cpu(owner),address);SH2_Read_Long(cpu(owner),port(owner,4)); }
        else { M68K_RB(address);M68K_RW(address); }
        PWMFIFOTransfer_EndFrame(60);CHECK(!latest(0) && !latest(1));
    }
    // Word and split longword ports: each accepted L/R sample counts exactly once.
    for(int owner=0;owner<3;owner++) {
        reset_pwm();PWMFIFOTransfer_BeginFrame();bus_write(owner,port(owner,4),4,0x01230234);PWMFIFOTransfer_EndFrame(60);
        CHECK(latest(1)==4 && !latest(0) && PWM_FIFO_L[0]==0x123 && PWM_FIFO_R[0]==0x234);
        // Left full, right empty: only the right half of a stereo write is accepted.
        bus_write(owner,port(owner,4),2,1);bus_write(owner,port(owner,4),2,2);PWM_RP_R=PWM_WP_R;
        PWMFIFOTransfer_BeginFrame();bus_write(owner,port(owner,4),4,0x03450456);PWMFIFOTransfer_EndFrame(60);
        CHECK(latest(1)==2 && PWM_FIFO_R[1]==0x456);
    }
    // Keep the existing mono-port behavior when the right FIFO is independently full.
    reset_pwm();for(unsigned i=0;i<3;i++)M68K_WW(port(0,6),(unsigned short)i);
    PWMFIFOTransfer_BeginFrame();M68K_WW(port(0,8),0x234);PWMFIFOTransfer_EndFrame(60);
    CHECK(latest(1)==4 && PWM_FIFO_L[0]==0x234 && PWM_FIFO_R[0]==0x234 && PWM_WP_L==1 && PWM_WP_R==1);
    puts("PASS: 68000/master/slave ports and mirrors, byte latch/word/stereo/mono payload, full/empty, zero samples, FIFO wrap and status polling");return 0;
}
static void put(unsigned offset,unsigned op) { _32X_Ram[offset]=(unsigned char)(op>>8);_32X_Ram[offset+1]=(unsigned char)op; }
static int instruction_tests(void) {
    const unsigned ops[]={0x1280,0x3280,0x2280};
    for(int owner=0;owner<3;owner++)for(unsigned size=0;size<3;size++) {
        reset_pwm();unsigned address=port(owner,4)+(size==0);
        if(owner) {
            SH2_CONTEXT *c=cpu(owner);SH2_Reset(c,0);c->Status=0;SH2_Set_SR(c,0xf0);SH2_Clear_Odo(c);SH2_Set_PC(c,0x06000004);
            c->R[0]=0x01230234;c->R[1]=address;put(0,0x2100+size);put(2,0x001b);
            PWMFIFOTransfer_BeginFrame();SH2_Exec(c,100);PWMFIFOTransfer_EndFrame(60);
        } else {
            M68K_WW(0xff0200,(unsigned short)ops[size]);main68k_context.dreg[0]=0x01230234;main68k_context.areg[1]=address;
            main68k_context.pc=0xff0200;main68k_context.sr=0x2700;memset(main68k_context.interrupts,0,sizeof(main68k_context.interrupts));
            main68k_tripOdometer();PWMFIFOTransfer_BeginFrame();main68k_exec(1);PWMFIFOTransfer_EndFrame(60);
        }
        CHECK(latest(1)==(size==2?4:2) && !latest(0));
        CHECK(PWM_FIFO_L[0]==(size==2?0x123:size==1?0x234:0x34));if(size==2)CHECK(PWM_FIFO_R[0]==0x234);
    }
    puts("PASS: actual 68000 and both SH2 byte/word/long store instructions with unchanged payload");return 0;
}
static int playback_tests(void) {
    const int left_dest[]={-1,0,1,-1,-1,0,1,-1,-1,0,1,-1,-1,0,1,-1};
    const int right_dest[]={-1,-1,-1,-1,0,1,0,0,1,1,0,1,-1,-1,-1,-1};
    for(unsigned mode=0;mode<16;mode++)for(unsigned ready=0;ready<4;ready++) {
        reset_pwm();PWM_Mode=mode;PWM_Out_L=0x55;PWM_Out_R=0x66;
        if(ready&1)M68K_WW(port(0,4),0x123);if(ready&2)M68K_WW(port(0,6),0x456);
        unsigned output[]={0x55,0x66},bytes=0;
        if((ready&1) && left_dest[mode]>=0) { bytes+=2;output[left_dest[mode]]=0x123; }
        if((ready&2) && right_dest[mode]>=0) { bytes+=2;output[right_dest[mode]]=0x456; }
        PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);
        CHECK(latest(0)==bytes && !latest(1) && PWM_Out_L==output[0] && PWM_Out_R==output[1]);
        unsigned l=PWM_RP_L,r=PWM_RP_R;
        PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);
        CHECK(!latest(0) && l==PWM_RP_L && r==PWM_RP_R); // no repeated reads from held output
    }
    reset_pwm();M68K_WW(port(0,8),0x123);PWM_Set_Cycle(1); // cycle becomes zero: stopped
    PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);CHECK(!latest(0) && !PWM_RP_L && !PWM_RP_R);
    PWM_Set_Cycle(32);PWMFIFOTransfer_BeginFrame();PWM_Update_Timer(0);PWM_Update_Timer(30);PWMFIFOTransfer_EndFrame(60);CHECK(latest(0)==4);
    // Host mixing reuses PWM_Out; it must never count another FIFO read.
    int lbuf[16]={0},rbuf[16]={0};int *buffers[]={lbuf,rbuf};
    PWMFIFOTransfer_BeginFrame();for(unsigned i=0;i<10;i++)PWM_Update(buffers,16);PWMFIFOTransfer_EndFrame(60);
    CHECK(!latest(0) && !latest(1) && lbuf[0]!=0);
    puts("PASS: all playback routing modes and empty-side combinations, timer boundaries/stopped cycle, held output and audio mixing exclusion");return 0;
}
static void configure(SH2_CONTEXT *c,unsigned ch,unsigned src,unsigned dst,unsigned count,unsigned control) {
    unsigned reg=0xffffff80+ch*16;SH2_Write_Long(c,reg+12,0);SH2_Write_Long(c,reg,src);SH2_Write_Long(c,reg+4,dst);
    SH2_Write_Long(c,reg+8,count);SH2_Write_Long(c,0xffffffb0,1);SH2_Write_Long(c,reg+12,control);
}
static int dma_tests(void) {
    for(int owner=1;owner<=2;owner++)for(unsigned ch=0;ch<2;ch++)for(unsigned size=0;size<4;size++) {
        reset_pwm();SH2_CONTEXT *c=cpu(owner);SH2_Write_Long(c,0x06001000,0x01230234);
        unsigned width=size==3?4:1<<size,units=size==3?4:2,dst=port(owner,4)+(size==0);
        PWMFIFOTransfer_BeginFrame();configure(c,ch,0x06001000,dst,size==3?1:2,0x201|(size<<10));PWMFIFOTransfer_EndFrame(60);
        unsigned accepted=units>3?3:units;
        CHECK(!latest(0) && latest(1)==accepted*(width==4?4:2));
        // Filling both queues makes subsequent DMA attempts transfer no FIFO payload.
        while(PWM_WP_L!=3)bus_write(owner,port(owner,4),2,0);while(PWM_WP_R!=3)bus_write(owner,port(owner,6),2,0);
        PWMFIFOTransfer_BeginFrame();configure(c,ch,0x06001000,port(owner,4),2,0xa01);PWMFIFOTransfer_EndFrame(60);CHECK(!latest(1));
        reset_pwm();PWMFIFOTransfer_BeginFrame();configure(c,ch,0x06001000,port(owner,8),2,0x601);PWMFIFOTransfer_EndFrame(60);CHECK(latest(1)==8);
    }
    for(int owner=1;owner<=2;owner++) {
        reset_pwm();SH2_CONTEXT *c=cpu(owner);for(unsigned i=0;i<4;i++)SH2_Write_Long(c,0x06001000+i*4,0x03000400+i*0x10001);
        configure(c,1,0x06001000,port(owner,4),4,0x18e1);PWM_Mode=0x185;
        PWMFIFOTransfer_BeginFrame();PWMFIFOTransfer_EndFrame(60);CHECK(!latest(0) && !latest(1));
        for(unsigned i=0;i<6;i++) {
            PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);
            CHECK(latest(1)==(i<4?4:0) && latest(0)==(i>0 && i<5?4:0));
            CHECK(c->TCR1==(i<4?3-i:0));
        }
        CHECK(PWM_Out_L==0x303 && PWM_Out_R==0x403);
    }
    // Both SH2s receive the same PWM request; count both accepted DMA destinations.
    reset_pwm();for(int owner=1;owner<=2;owner++) { SH2_Write_Long(cpu(owner),0x06001000,0x01230234);configure(cpu(owner),1,0x06001000,port(owner,4),1,0x08e1); }
    PWM_Mode=0x185;PWMFIFOTransfer_BeginFrame();step();PWMFIFOTransfer_EndFrame(60);CHECK(!latest(0) && latest(1)==8);
    PWMFIFOTransfer_BeginFrame();step();step();PWMFIFOTransfer_EndFrame(60);CHECK(latest(0)==8 && !latest(1));
    reset_pwm();SH2_Write_Word(&M_SH2,0x06001000,1);
    PWMFIFOTransfer_BeginFrame();configure(&M_SH2,0,port(1,4),0x06001000,2,0x601);PWMFIFOTransfer_EndFrame(60);
    CHECK(!latest(0) && !latest(1)); // DMA status reads are not dequeues either.
    puts("PASS: both SH2 DMA channels/sizes, mono/stereo, full rejection, PWM paced pending/partial/completed requests, simultaneous CPU DMA and status-read exclusion");return 0;
}
// Use native transfer sites with controlled FIFO fixtures for independent rate histories.
static void sample(unsigned reads,unsigned writes,unsigned fps=60) {
    PWM_Mode=1;PWM_Enable=1;PWMFIFOTransfer_BeginFrame();
    for(unsigned i=0;i<reads;i+=2) { PWM_RP_L=0;PWM_WP_L=1;PWM_RP_R=PWM_WP_R=0;PWM_FIFO_L[0]=0x123;step(); }
    for(unsigned i=0;i<writes;i+=2) { PWM_RP_L=PWM_WP_L=0;M68K_WW(port(0,4),0x123); }
    PWMFIFOTransfer_EndFrame(fps);
}
static int history_tests(void) {
    reset_pwm();M68K_WW(port(0,8),1);step();PWMFIFOTransfer_EndFrame(60);CHECK(!PWMFIFOTransfer_GetHistory(0,values));
    PWMFIFOTransfer_BeginFrame();M68K_WB(port(0,4),0x12);M68K_WW(port(0),0);M68K_WW(port(0,2),32);
    M68K_WW(0xff1000,1);SH2_Write_Long(&M_SH2,0x06001000,1);PWMFIFOTransfer_EndFrame(60);CHECK(!latest(0) && !latest(1));
    reset_pwm();for(unsigned i=0;i<300;i++)sample(i*2,(300-i)*2);
    CHECK(PWMFIFOTransfer_GetHistory(0,values)==256 && values[0]==88 && values[255]==598);
    CHECK(PWMFIFOTransfer_GetHistory(1,values)==256 && values[0]==512 && values[255]==2);
    for(unsigned fps=50;fps<=60;fps+=10) {
        reset_pwm();for(unsigned i=0;i<fps;i++)sample(1024,2048,fps);
        CHECK(PWMFIFOTransfer_GetBytesPerSecond(0)==fps*1024 && PWMFIFOTransfer_GetBytesPerSecond(1)==fps*2048);
        for(unsigned i=0;i<fps;i++)sample(0,0,fps);CHECK(!PWMFIFOTransfer_GetBytesPerSecond(0) && !PWMFIFOTransfer_GetBytesPerSecond(1));
    }
    sample(2,4,50);PWMFIFOTransfer_EndFrame(50);CHECK(PWMFIFOTransfer_GetHistory(0,values)==1);
    PWM_Init();CHECK(!PWMFIFOTransfer_GetHistory(0,values));reset_pwm();
    _32X_Started=0;Genesis_Started=1;sample(2,4);CHECK(!PWMFIFOTransfer_GetHistory(0,values));
    Genesis_Started=0;SegaCD_Started=1;sample(2,4);CHECK(!PWMFIFOTransfer_GetHistory(0,values));
    SegaCD_Started=0;CD_32X_Active=1;sample(2,4);CHECK(latest(0)==2 && latest(1)==4);CD_32X_Active=0;
    puts("PASS: independent histories/rates, PAL/NTSC, idle decay, outside-frame/staging/config exclusion, PWM reset and inactive modes");return 0;
}
static int panel_tests(void) {
    char folder[MAX_PATH], config[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"vrt",0,config));
    _32X_Started=1; Genesis_Started=SegaCD_Started=CD_32X_Active=0; Paused=0;
    reset_pwm(); for (unsigned i=0;i<256;i++) sample(1024,2048);
    pwmfifotransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_PWM_FIFO_TRANSFER-1]; CHECK(panel);
    pwmfifotransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    pwmfifotransfer_update();
    GetDlgItemText(panel,IDC_PWM_FIFO_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0 KiB/s"));
    GetDlgItemText(panel,IDC_PWM_FIFO_WRITE_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HDC screen=GetDC(panel), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hDC=dc; SetRect(&item.rcItem,0,0,500,180);
    const int graphs[]={IDC_PWM_FIFO_READ_GRAPH,IDC_PWM_FIFO_WRITE_GRAPH};
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
    for (unsigned i=0;i<10;i++) pwmfifotransfer_update();
    CHECK(PWMFIFOTransfer_GetHistory(0,values)==256 && latest(0)==1024 && latest(1)==8192);
    GetDlgItemText(panel,IDC_PWM_FIFO_TRANSFER_STATUS,text,sizeof(text)); CHECK(strstr(text,"Left + Right") && strstr(text,"Paused"));
    _32X_Started=0; pwmfifotransfer_update();
    GetDlgItemText(panel,IDC_PWM_FIFO_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"--"));
    SendMessage(panel,WM_DRAWITEM,graphs[0],(LPARAM)&item); CHECK(GetPixel(dc,100,145)==GetSysColor(COLOR_WINDOW));
    CD_32X_Active=1; pwmfifotransfer_update();
    GetDlgItemText(panel,IDC_PWM_FIFO_READ_RATE,text,sizeof(text)); CHECK(strstr(text,"60.0"));
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
    OpenWindow_KMod(DMODE_PWM_FIFO_TRANSFER); GetWindowRect(panel,&saved); pwmfifotransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !PWMFIFOTransfer_GetHistory(0,values));
    pwmfifotransfer_destroy(); pwmfifotransfer_create(GetModuleHandle(NULL),NULL); pwmfifotransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_PWM_FIFO_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    pwmfifotransfer_save_window(config); pwmfifotransfer_destroy();
    pwmfifotransfer_create(GetModuleHandle(NULL),NULL); pwmfifotransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_PWM_FIFO_TRANSFER-1]); pwmfifotransfer_destroy(); DeleteFile(config);
    CD_32X_Active=0;
    puts("PASS: separate graphs/rates, common scale, paused/inactive/CD32X states, layout and window persistence");
    return 0;
}

int main(void) {
    setbuf(stdout,NULL);M68K_Init();S68K_Init();Init_Tab();MSH2_Init();SSH2_Init();
    return enqueue_tests() || instruction_tests() || playback_tests() || dma_tests() || history_tests() || panel_tests();
}
