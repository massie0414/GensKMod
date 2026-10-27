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
#include "../src/Gens/vdp_32X.h"
#include "../src/Gens/vram32x_transfer.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/vram32x_transfer_panel.h"
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned long values[VRAM32X_TRANSFER_HISTORY];
static unsigned long latest(void) { unsigned n=VRAM32XTransfer_GetHistory(values);return n?values[n-1]:~0UL; }
static SH2_CONTEXT *sh2(int owner) { return owner==1?&M_SH2:&S_SH2; }
static void mapping(int owner,unsigned bank) { _32X_FM=owner?0x80:0;_32X_VDP.State=bank^1;_32X_Set_FB(); }
static void write_bus(int owner,unsigned address,unsigned width,unsigned value) {
    if(owner) {
        if(width==1)SH2_Write_Byte(sh2(owner),address,(unsigned char)value);
        else if(width==2)SH2_Write_Word(sh2(owner),address,(unsigned short)value);
        else SH2_Write_Long(sh2(owner),address,value);
    } else {
        if(width==1)M68K_WB(address,(unsigned char)value);
        else if(width==2)M68K_WW(address,(unsigned short)value);
        else { M68K_WW(address,(unsigned short)(value>>16));M68K_WW(address+2,(unsigned short)value); }
    }
}
static unsigned base(int owner) { return owner?0x04000000:0x840000; }
static void sample(unsigned bytes,unsigned fps=60) {
    mapping(0,0);VRAM32XTransfer_BeginFrame();
    for(unsigned i=0;i<bytes;i++)M68K_WB(0x840100,0x12);
    VRAM32XTransfer_EndFrame(fps);
}
static int access_tests(void) {
    _32X_Started=1;Genesis_Started=SegaCD_Started=CD_32X_Active=0;Init_Memory_M68K(1);
    for(int owner=0;owner<3;owner++)for(unsigned bank=0;bank<2;bank++)for(unsigned mode=0;mode<2;mode++)
    for(unsigned size=0;size<3;size++)for(unsigned alias=0;alias<2;alias++) {
        unsigned width=1<<size;mapping(owner,bank);
        unsigned addr=base(owner)+0x100+(mode?0x20000:0)+(alias?(owner?0x20040000:0x80000):0);
        for(unsigned mask=0;mask<(1u<<width);mask++) {
            unsigned data=0,expected=0;
            for(unsigned i=0;i<width;i++) { data=(data<<8)|((mask&(1<<i))?0x11+i:0);if(mask&(1<<i))++expected; }
            if(!mode && width!=1)expected=width;
            memset(_32X_VDP_Ram,0xa5,sizeof(_32X_VDP_Ram));
            VRAM32XTransfer_BeginFrame();write_bus(owner,addr,width,data);VRAM32XTransfer_EndFrame(60);
            CHECK(latest()==expected);
            for(unsigned i=0;i<width;i++) {
                unsigned byte=(data>>((width-1-i)*8))&255;
                unsigned char wanted=(mode || width==1) && !byte ? 0xa5 : (unsigned char)byte;
                CHECK(_32X_VDP_Ram[bank*0x20000+((0x100+i)^1)]==wanted);
                CHECK(_32X_VDP_Ram[(1-bank)*0x20000+((0x100+i)^1)]==0xa5);
            }
            // Writing unchanged values is still a transfer.
            VRAM32XTransfer_BeginFrame();write_bus(owner,addr,width,data);VRAM32XTransfer_EndFrame(60);CHECK(latest()==expected);
        }
    }
    // Ownership-denied stores and all framebuffer reads are excluded.
    for(unsigned bank=0;bank<2;bank++)for(int owner=0;owner<3;owner++) {
        mapping(owner?0:1,bank);memset(_32X_VDP_Ram,0xa5,sizeof(_32X_VDP_Ram));
        VRAM32XTransfer_BeginFrame();write_bus(owner,base(owner)+0x100,4,0x12345678);VRAM32XTransfer_EndFrame(60);
        CHECK(!latest());CHECK(_32X_VDP_Ram[bank*0x20000+0x100]==0xa5);
        mapping(owner,bank);VRAM32XTransfer_BeginFrame();
        if(owner) { SH2_Read_Byte(sh2(owner),base(owner));SH2_Read_Word(sh2(owner),base(owner));SH2_Read_Long(sh2(owner),base(owner)); }
        else { M68K_RB(base(owner));M68K_RW(base(owner)); }
        VRAM32XTransfer_EndFrame(60);CHECK(!latest());
    }
    puts("PASS: 68000/master/slave writes, both framebuffers, aliases, byte/word/long, every transparent-byte mask, repeated writes, ownership and read exclusion");return 0;
}
static void put(unsigned offset,unsigned op) { _32X_Ram[offset]=(unsigned char)(op>>8);_32X_Ram[offset+1]=(unsigned char)op; }
static void run68k(unsigned pc=0xff0200) {
    main68k_context.pc=pc;main68k_context.sr=0x2700;memset(main68k_context.interrupts,0,sizeof(main68k_context.interrupts));
    main68k_tripOdometer();main68k_exec(1);
}
static int instruction_tests(void) {
    const unsigned ops[]={0x1280,0x3280,0x2280};
    for(int owner=0;owner<3;owner++)for(unsigned bank=0;bank<2;bank++)for(unsigned size=0;size<3;size++) {
        unsigned width=1<<size;
        if(owner) {
            SH2_CONTEXT *cpu=sh2(owner);SH2_Reset(cpu,0);cpu->Status=0;SH2_Set_SR(cpu,0xf0);SH2_Clear_Odo(cpu);
            SH2_Set_PC(cpu,0x06000004);cpu->R[0]=0x12345678;cpu->R[1]=0x24000100;
            put(0,0x2100+size);put(2,0x001b);mapping(owner,bank);
            VRAM32XTransfer_BeginFrame();SH2_Exec(cpu,100);VRAM32XTransfer_EndFrame(60);
        } else {
            M68K_WW(0xff0200,(unsigned short)ops[size]);main68k_context.dreg[0]=0x12345678;main68k_context.areg[1]=0x840100;mapping(owner,bank);
            VRAM32XTransfer_BeginFrame();run68k();VRAM32XTransfer_EndFrame(60);
        }
        CHECK(latest()==width);
    }
    mapping(0,1);M68K_WW(0xff0200,0x48d0);M68K_WW(0xff0202,7); // MOVEM.L D0-D2,(A0)
    main68k_context.areg[0]=0x840100;main68k_context.dreg[0]=0x12345678;main68k_context.dreg[1]=0;main68k_context.dreg[2]=0xaabbccdd;
    VRAM32XTransfer_BeginFrame();run68k();VRAM32XTransfer_EndFrame(60);CHECK(latest()==12);
    CHECK(M68K_RW(0x840100)==0x1234 && M68K_RW(0x84010a)==0xccdd);
    // Both CPUs and banks aggregate into a single sample.
    VRAM32XTransfer_BeginFrame();mapping(0,0);write_bus(0,0x840100,2,1);mapping(1,1);write_bus(1,0x04000100,4,2);
    write_bus(2,0x24000100,1,3);VRAM32XTransfer_EndFrame(60);CHECK(latest()==7);
    puts("PASS: real 68000 and SH2 store instructions, MOVEM/longword payload and combined totals");return 0;
}
static int fill_tests(void) {
    const unsigned lengths[]={0,1,2,3,254,255};
    for(int owner=0;owner<3;owner++)for(unsigned bank=0;bank<2;bank++)for(unsigned i=0;i<6;i++) {
        mapping(owner,bank);unsigned reg=owner?0x20004100:0xa15180;
        memset(_32X_VDP_Ram,0xa5,sizeof(_32X_VDP_Ram));
        VRAM32XTransfer_BeginFrame();write_bus(owner,reg+4,2,lengths[i]);write_bus(owner,reg+6,2,0x1200);VRAM32XTransfer_EndFrame(60);
        CHECK(!latest());
        VRAM32XTransfer_BeginFrame();write_bus(owner,reg+8,2,0);VRAM32XTransfer_EndFrame(60);
        // Match actual native stores, including the existing 68000 fill loop's extra pair.
        unsigned words=lengths[i]+1+(!owner && lengths[i]?2:0);
        CHECK(latest()==words*2);CHECK(_32X_VDP_Ram[bank*0x20000+0x2400]==0);
        CHECK(_32X_VDP_Ram[(1-bank)*0x20000+0x2400]==0xa5);
        VRAM32XTransfer_BeginFrame();write_bus(owner,reg+8,2,0);VRAM32XTransfer_EndFrame(60);CHECK(latest()==words*2);
        mapping(owner?0:1,bank);VRAM32XTransfer_BeginFrame();write_bus(owner,reg+8,2,0x1234);VRAM32XTransfer_EndFrame(60);CHECK(!latest());
    }
    // Address low-byte wrap and final address follow the same existing fill operation.
    mapping(1,0);SH2_Write_Word(&M_SH2,0x20004104,3);SH2_Write_Word(&M_SH2,0x20004106,0x12fe);
    VRAM32XTransfer_BeginFrame();SH2_Write_Word(&M_SH2,0x20004108,0x1234);VRAM32XTransfer_EndFrame(60);
    CHECK(latest()==8 && _32X_VDP.AF_St==0x1202);
    CHECK(((unsigned short*)_32X_VDP_Ram)[0x12fe]==0x1234 && ((unsigned short*)_32X_VDP_Ram)[0x1200]==0x1234);
    puts("PASS: 68000/SH2 autofill actual stores, both banks, zero fills, full lengths, repeats, ownership and address wrap");return 0;
}
static void configure(SH2_CONTEXT *cpu,unsigned channel,unsigned src,unsigned dst,unsigned count,unsigned control) {
    unsigned reg=0xffffff80+channel*16;SH2_Write_Long(cpu,reg+12,0);SH2_Write_Long(cpu,reg,src);
    SH2_Write_Long(cpu,reg+4,dst);SH2_Write_Long(cpu,reg+8,count);SH2_Write_Long(cpu,0xffffffb0,1);SH2_Write_Long(cpu,reg+12,control);
}
static SH2_CONTEXT *request_cpu;static unsigned request_channel,request_reads;
static unsigned long FASTCALL request_read(unsigned long) {
    if(++request_reads==3) { if(request_channel)request_cpu->DREQ1=0;else request_cpu->DREQ0=0; }
    return 0x00120034;
}
static int dma_tests(void) {
    for(int owner=1;owner<3;owner++)for(unsigned bank=0;bank<2;bank++)for(unsigned ch=0;ch<2;ch++)
    for(unsigned size=0;size<4;size++)for(unsigned sm=0;sm<3;sm++)for(unsigned dm=0;dm<3;dm++) {
        mapping(owner,bank);SH2_CONTEXT *cpu=sh2(owner);unsigned width=size==3?4:1<<size,units=size==3?8:2;
        memset(_32X_Ram+0x1000,0x12,64);unsigned src=0x06001000+(sm==2?units*width:0),dst=0x24000100+(dm==2?units*width:0);
        VRAM32XTransfer_BeginFrame();configure(cpu,ch,src,dst,2,0x201|(size<<10)|(sm<<12)|(dm<<14));VRAM32XTransfer_EndFrame(60);
        CHECK(latest()==units*width && ((unsigned char*)_32X_VDP_Ram)[bank*0x20000+0x101]==0x12);
    }
    for(int owner=1;owner<3;owner++)for(unsigned ch=0;ch<2;ch++) {
        SH2_CONTEXT *cpu=sh2(owner);mapping(owner,0);cpu->DREQ0=cpu->DREQ1=0;
        // Overwrite DMA writes only the two nonzero bytes of each longword.
        SH2_Write_Long(cpu,0x06001000,0x00120034);
        VRAM32XTransfer_BeginFrame();configure(cpu,ch,0x06001000,0x04020100,3,0x0a01);VRAM32XTransfer_EndFrame(60);CHECK(latest()==6);
        // Pending and partial requests are distinct frames, including a partial resume.
        SH2_Add_ReadL(cpu,0x10,0x10,request_read);request_cpu=cpu;request_channel=ch;request_reads=0;
        VRAM32XTransfer_BeginFrame();configure(cpu,ch,0x10000000,0x24020100,8,0x0801);VRAM32XTransfer_EndFrame(60);CHECK(!latest());
        for(unsigned part=0;part<2;part++) {
            request_reads=0;VRAM32XTransfer_BeginFrame();if(ch)SH2_DMA1_Request(cpu,1);else SH2_DMA0_Request(cpu,1);VRAM32XTransfer_EndFrame(60);
            CHECK(latest()==6 && (ch?cpu->TCR1:cpu->TCR0)==8-(part+1)*3);
        }
        SH2_Write_Long(cpu,0xffffff8c+ch*16,0);
        mapping(0,0);VRAM32XTransfer_BeginFrame();configure(cpu,ch,0x06001000,0x04000100,2,0x0a01);VRAM32XTransfer_EndFrame(60);CHECK(!latest());
    }
    // Framebuffer reads into SDRAM are not VRAM write traffic.
    mapping(1,0);VRAM32XTransfer_BeginFrame();configure(&M_SH2,0,0x04000100,0x06001000,3,0x0a01);VRAM32XTransfer_EndFrame(60);CHECK(!latest());
    puts("PASS: SH2 DMA both CPUs/channels/banks, sizes/address modes, transparent bytes, pending/partial/resumed requests and read-only exclusion");return 0;
}
static int history_tests(void) {
    VRAM32XTransfer_Reset();mapping(0,0);M68K_WW(0x840100,0);VRAM32XTransfer_EndFrame(60);CHECK(!VRAM32XTransfer_GetHistory(values));
    VRAM32XTransfer_BeginFrame();M68K_WW(0xa15200,0x1234);M68K_WW(0xff1000,0x1234);SH2_Write_Long(&M_SH2,0x06001000,0);
    VRAM32XTransfer_EndFrame(60);CHECK(!latest());
    for(unsigned i=0;i<300;i++)sample(i);CHECK(VRAM32XTransfer_GetHistory(values)==256 && values[0]==44 && values[255]==299);
    for(unsigned fps=50;fps<=60;fps+=10) {
        VRAM32XTransfer_Reset();for(unsigned i=0;i<fps;i++)sample(1024,fps);CHECK(VRAM32XTransfer_GetBytesPerSecond()==fps*1024);
        for(unsigned i=0;i<fps;i++)sample(0,fps);CHECK(!VRAM32XTransfer_GetBytesPerSecond());
    }
    sample(4,50);VRAM32XTransfer_EndFrame(50);CHECK(VRAM32XTransfer_GetHistory(values)==1 && latest()==4);
    _32X_VDP_Reset();CHECK(!VRAM32XTransfer_GetHistory(values));
    _32X_Started=0;Genesis_Started=1;sample(4);CHECK(!VRAM32XTransfer_GetHistory(values));
    Genesis_Started=0;SegaCD_Started=1;sample(4);CHECK(!VRAM32XTransfer_GetHistory(values));
    SegaCD_Started=0;CD_32X_Active=1;sample(4);CHECK(latest()==4);CD_32X_Active=0;
    puts("PASS: histories/rates, PAL/NTSC, idle decay, reset/initialization, other memory and inactive modes");return 0;
}
static int panel_tests(void) {
    char config[MAX_PATH], folder[MAX_PATH], text[160]; RECT saved, restored;
    GetTempPath(sizeof(folder),folder); CHECK(GetTempFileName(folder,"cdt",0,config));
    _32X_Started=1; CD_32X_Active=0; Paused=0;
    vram32xtransfer_create(GetModuleHandle(NULL),NULL);
    HWND panel=HandleWindow_KMod[DMODE_VRAM32X_TRANSFER-1]; CHECK(panel);
    vram32xtransfer_restore_window(config); CHECK(!IsWindowVisible(panel));
    VRAM32XTransfer_Reset(); for (unsigned i=0;i<256;i++) sample(2048);
    vram32xtransfer_update(); GetDlgItemText(panel,IDC_VRAM32X_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0 KiB/s"));
    HWND graph=GetDlgItem(panel,IDC_VRAM32X_TRANSFER_GRAPH);
    HDC screen=GetDC(graph), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,500,180);
    HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hwndItem=graph; item.hDC=dc;
    SetRect(&item.rcItem,0,0,500,180);
    SendMessage(panel,WM_DRAWITEM,IDC_VRAM32X_TRANSFER_GRAPH,(LPARAM)&item);
    CHECK(GetPixel(dc,100,40)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,140)==RGB(0,0,255));
    Paused=1;
    for (unsigned i=0;i<10;i++) { vram32xtransfer_update(); SendMessage(panel,WM_DRAWITEM,IDC_VRAM32X_TRANSFER_GRAPH,(LPARAM)&item); }
    CHECK(VRAM32XTransfer_GetHistory(values)==256 && values[255]==2048);
    GetDlgItemText(panel,IDC_VRAM32X_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"Paused"));
    _32X_Started=0; vram32xtransfer_update(); SendMessage(panel,WM_DRAWITEM,IDC_VRAM32X_TRANSFER_GRAPH,(LPARAM)&item);
    CHECK(GetPixel(dc,100,140)==GetSysColor(COLOR_WINDOW));
    GetDlgItemText(panel,IDC_VRAM32X_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"inactive"));
    CD_32X_Active=1; vram32xtransfer_update(); GetDlgItemText(panel,IDC_VRAM32X_TRANSFER_RATE,text,sizeof(text)); CHECK(strstr(text,"120.0"));
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
    OpenWindow_KMod(DMODE_VRAM32X_TRANSFER); GetWindowRect(panel,&saved); vram32xtransfer_save_window(config);
    kmod_close(); CHECK(IsWindowVisible(panel) && !VRAM32XTransfer_GetHistory(values));
    vram32xtransfer_destroy(); CHECK(!HandleWindow_KMod[DMODE_VRAM32X_TRANSFER-1]);
    vram32xtransfer_create(GetModuleHandle(NULL),NULL); vram32xtransfer_restore_window(config);
    panel=HandleWindow_KMod[DMODE_VRAM32X_TRANSFER-1]; GetWindowRect(panel,&restored);
    CHECK(IsWindowVisible(panel) && EqualRect(&saved,&restored));
    SendMessage(panel,WM_COMMAND,IDCANCEL,0); CHECK(!IsWindowVisible(panel));
    vram32xtransfer_save_window(config); vram32xtransfer_destroy();
    vram32xtransfer_create(GetModuleHandle(NULL),NULL); vram32xtransfer_restore_window(config);
    CHECK(!OpenedWindow_KMod[DMODE_VRAM32X_TRANSFER-1]); vram32xtransfer_destroy(); DeleteFile(config);
    puts("PASS: blue graph, inactive/CD32X/paused display, layout, position/visibility persistence and ROM close");
    return 0;
}

int main(void) {
    setbuf(stdout,NULL);M68K_Init();S68K_Init();Init_Tab();MSH2_Init();SSH2_Init();
    return access_tests() || instruction_tests() || fill_tests() || dma_tests() || history_tests() || panel_tests();
}
