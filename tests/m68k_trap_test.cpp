#include <stdio.h>
#include <string.h>
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Mem_M68k.h"
#include "../src/Gens/Mem_S68k.h"
extern "C" {
#include "../src/Gens/Gens.h"
}
#include "../src/Gens/resource.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/kmod/s68k.h"
#include "../src/Gens/kmod/m68k.h"
extern "C" {
#include "../src/Gens/kmod/common.h"
}
extern "C" BOOL CALLBACK CD_68KDlgProc(HWND, UINT, WPARAM, LPARAM);
extern "C" BOOL CALLBACK M68KDlgProc(HWND, UINT, WPARAM, LPARAM);

static int resets;
static void reset_hook(void) { ++resets; }
static unsigned char *memory=Rom_Data;
static void word(unsigned a,unsigned v) { memory[a]=v&255; memory[a+1]=v>>8; }
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
 M68K_Init(); Init_Tab(); Rom_Size=0x10000; word(0,0x00ff); word(2,0xf000); word(4,0); word(6,0x200); M68K_Reset(0);
 S68K_Init(); S68K_Reset();
 for(int sub=0;sub<2;sub++) {
  memory=sub?Ram_Prg:Rom_Data;
  for(unsigned n=0;n<16;n++) {
   memset(memory,0,65536);
   S68000CONTEXT c=sub?sub68k_context:main68k_context;
   const unsigned stack=sub?0x7000:0xfff000;
   c.resethandler=reset_hook; c.pc=0x200; c.areg[7]=stack; c.sr=0x2700;
   unsigned handler=0x400+n*4;
   word(0x80+4*n,0); word(0x82+4*n,handler);
   word(0x200,0x4e40+n); word(handler,0x4e73);
   if(sub) { sub68k_SetContext(&c); sub68k_tripOdometer(); sub68k_exec(1); }
   else { main68k_SetContext(&c); main68k_tripOdometer(); main68k_exec(1); }
   S68000CONTEXT *result=sub?&sub68k_context:&main68k_context;
   CHECK(result->pc==handler); CHECK(resets==0);
   CHECK(result->areg[7]==stack-6);
   unsigned short (*read)(unsigned)=sub?S68K_RW:M68K_RW;
   CHECK(read(stack-6)==0x2700 && read(stack-4)==0 && read(stack-2)==0x202);
   if(sub) { sub68k_tripOdometer(); sub68k_exec(1); }
   else { main68k_tripOdometer(); main68k_exec(1); }
   CHECK(result->pc==0x202 && result->areg[7]==stack);
  }
 }

 {
 /* Usage is based on emulated cycles, including partial STOP slices. */
 memory=Ram_Prg;
 S68K_Reset();
 S68000CONTEXT c=sub68k_context;
 c.pc=0x200; c.sr=0x2700; c.interrupts[0]=0;
 for(unsigned a=0x200; a<0x1000; a+=2)word(a,0x4e71); /* NOP */
 sub68k_SetContext(&c); sub68k_tripOdometer(); S68K_Usage_Begin_Frame();
 sub68k_exec(1000); S68K_Usage_End_Frame(1000);
 unsigned long values[S68K_USAGE_HISTORY];
 CHECK(S68K_Usage_GetHistory(values)==1 && values[0]==1000);
 word(0x200,0x4e72); word(0x202,0x2700); /* STOP #$2700 */
 c.pc=0x200; c.interrupts[0]=0;
 sub68k_SetContext(&c); sub68k_tripOdometer(); S68K_Usage_Begin_Frame();
 sub68k_exec(1000); S68K_Usage_End_Frame(1000);
 CHECK(S68K_Usage_GetHistory(values)==2 && values[1]>=4 && values[1]<=5);
 sub68k_tripOdometer(); S68K_Usage_Begin_Frame();
 sub68k_exec(1000); S68K_Usage_End_Frame(1000);
 CHECK(S68K_Usage_GetHistory(values)==3 && values[2]==0);
 for(unsigned n=0;n<300;n++) {
  sub68k_tripOdometer(); S68K_Usage_Begin_Frame();
  sub68k_exec(1000); S68K_Usage_End_Frame(1000);
 }
 CHECK(S68K_Usage_GetHistory(values)==256);
 for(unsigned n=0;n<256;n++)CHECK(values[n]==0);
 /* Bus-held CPU has no execution; repeated reads must not advance history. */
 sub68k_tripOdometer(); S68K_Usage_Begin_Frame(); S68K_Usage_End_Frame(1000);
 CHECK(S68K_Usage_GetHistory(values)==256 && values[255]==0);
 CHECK(S68K_Usage_GetHistory(values)==256);
 S68K_Reset_CPU(); CHECK(S68K_Usage_GetHistory(values)==0);
 puts("PASS: SegaCD usage: busy, STOP transition, stopped/bus-held, 256-frame ring, reset");

 /* Exercise the real dialog resource, font fitting and owner-drawn graph. */
 HWND panel=CreateDialog(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DEBUGCD_68K),NULL,CD_68KDlgProc);
 CHECK(panel);
 const int ids[]={IDC_S68K_STATUS_SR,IDC_S68K_STATUS_ADR,IDC_S68K_STATUS_DATA};
 const char *texts[]={"X=1 N=1 Z=1 V=1 C=1  SR=FFFF Cycles=4294967295",
  "A0=FFFFFFFF A1=FFFFFFFF A2=FFFFFFFF A3=FFFFFFFF\nA4=FFFFFFFF A5=FFFFFFFF A6=FFFFFFFF A7=FFFFFFFF",
  "D0=FFFFFFFF D1=FFFFFFFF D2=FFFFFFFF D3=FFFFFFFF\nD4=FFFFFFFF D5=FFFFFFFF D6=FFFFFFFF D7=FFFFFFFF"};
 RECT group,client,previous={0};
 GetWindowRect(GetDlgItem(panel,IDC_S68K_USAGE_GROUP),&group);
 GetClientRect(panel,&client); MapWindowPoints(panel,NULL,(POINT*)&client,2);
 CHECK(group.right<=client.right && group.bottom<=client.bottom);
 for(unsigned i=0;i<3;i++) {
  HWND control=GetDlgItem(panel,ids[i]); RECT bounds,required={0},overlap;
  HDC dc=GetDC(control); HGDIOBJ old=SelectObject(dc,(HFONT)SendMessage(control,WM_GETFONT,0,0));
  DrawText(dc,texts[i],-1,&required,DT_CALCRECT|DT_LEFT|DT_NOPREFIX);
  SelectObject(dc,old); ReleaseDC(control,dc); GetWindowRect(control,&bounds);
  CHECK(bounds.right-bounds.left>=required.right && bounds.bottom-bounds.top>=required.bottom);
  CHECK(bounds.bottom<=client.bottom && !IntersectRect(&overlap,&bounds,&group));
  if(i)CHECK(bounds.top>=previous.bottom);
  previous=bounds;
 }
 SegaCD_Started=1;
 for(unsigned i=0;i<256;i++) { sub68k_context.odometer=500; S68K_Usage_Begin_Frame(); S68K_Usage_End_Frame(1000); }
 HWND graph=GetDlgItem(panel,IDC_S68K_USAGE);
 HDC screen=GetDC(graph), dc=CreateCompatibleDC(screen);
 HBITMAP bitmap=CreateCompatibleBitmap(screen,300,140); HGDIOBJ old=SelectObject(dc,bitmap);
 DRAWITEMSTRUCT item={0}; item.hwndItem=graph; item.hDC=dc; SetRect(&item.rcItem,0,0,300,140);
 SendMessage(panel,WM_DRAWITEM,IDC_S68K_USAGE,(LPARAM)&item);
 CHECK(GetPixel(dc,100,30)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,110)==RGB(0,0,255));
 SegaCD_Started=0; SendMessage(panel,WM_DRAWITEM,IDC_S68K_USAGE,(LPARAM)&item);
 CHECK(GetPixel(dc,100,110)==GetSysColor(COLOR_WINDOW));
 CHECK(S68K_Usage_GetHistory(values)==256); /* repaint never adds samples */
 SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(graph,screen);
 char dir[MAX_PATH], config[MAX_PATH]; RECT saved, restored; MSG msg;
 CHECK(GetTempPath(sizeof(dir),dir) && GetTempFileName(dir,"s68",0,config));
 s68kdebug_restore_window(config);
 CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_CD_68K-1]);
 ShowWindow(panel,SW_SHOW); OpenedWindow_KMod[DMODE_CD_68K-1]=TRUE;
 GetWindowRect(panel,&saved); s68kdebug_save_window(config); s68kdebug_destroy();
 CHECK(!HandleWindow_KMod[DMODE_CD_68K-1]);
 CHECK(!PeekMessage(&msg,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
 s68kdebug_create(GetModuleHandle(NULL),NULL); s68kdebug_restore_window(config);
 panel=HandleWindow_KMod[DMODE_CD_68K-1]; GetWindowRect(panel,&restored);
 CHECK(IsWindowVisible(panel) && OpenedWindow_KMod[DMODE_CD_68K-1] && EqualRect(&saved,&restored));
 SendMessage(panel,WM_CLOSE,0,0);
 CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_CD_68K-1]);
 s68kdebug_save_window(config); s68kdebug_destroy();
 s68kdebug_create(GetModuleHandle(NULL),NULL); s68kdebug_restore_window(config);
 panel=HandleWindow_KMod[DMODE_CD_68K-1];
 CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_CD_68K-1]);
 s68kdebug_destroy(); DeleteFile(config);
 puts("PASS: SegaCD window: default closed, restores open/closed state and bounds, no WM_QUIT");
 puts("PASS: SegaCD dialog: register bounds, graph placement, blue usage bars, inactive state");
 }
 {
 /* Usage is based on emulated cycles, including partial STOP slices. */
 memory=Rom_Data;
 M68K_Reset(0);
 S68000CONTEXT c=main68k_context;
 c.pc=0x200; c.sr=0x2700; c.interrupts[0]=0;
 for(unsigned a=0x200; a<0x1000; a+=2)word(a,0x4e71); /* NOP */
 main68k_SetContext(&c); main68k_tripOdometer(); M68K_Usage_Begin_Frame();
 main68k_exec(1000); M68K_Usage_End_Frame(1000);
 unsigned long values[M68K_USAGE_HISTORY];
 CHECK(M68K_Usage_GetHistory(values)==1 && values[0]==1000);
 word(0x200,0x4e72); word(0x202,0x2700); /* STOP #$2700 */
 c.pc=0x200; c.interrupts[0]=0;
 main68k_SetContext(&c); main68k_tripOdometer(); M68K_Usage_Begin_Frame();
 main68k_exec(1000); M68K_Usage_End_Frame(1000);
 CHECK(M68K_Usage_GetHistory(values)==2 && values[1]>=4 && values[1]<=5);
 main68k_tripOdometer(); M68K_Usage_Begin_Frame();
 main68k_exec(1000); M68K_Usage_End_Frame(1000);
 CHECK(M68K_Usage_GetHistory(values)==3 && values[2]==0);
 for(unsigned n=0;n<300;n++) {
  main68k_tripOdometer(); M68K_Usage_Begin_Frame();
  main68k_exec(1000); M68K_Usage_End_Frame(1000);
 }
 CHECK(M68K_Usage_GetHistory(values)==256);
 for(unsigned n=0;n<256;n++)CHECK(values[n]==0);
 /* Bus-held CPU has no execution; repeated reads must not advance history. */
 main68k_tripOdometer(); M68K_Usage_Begin_Frame(); M68K_Usage_End_Frame(1000);
 CHECK(M68K_Usage_GetHistory(values)==256 && values[255]==0);
 CHECK(M68K_Usage_GetHistory(values)==256);
 M68K_Reset_CPU(); CHECK(M68K_Usage_GetHistory(values)==0);
 puts("PASS: Genesis usage: busy, STOP transition, stopped/bus-held, 256-frame ring, reset");

 /* Exercise the real dialog resource, font fitting and owner-drawn graph. */
 HWND panel=CreateDialog(GetModuleHandle(NULL),MAKEINTRESOURCE(IDD_DEBUG68K),NULL,M68KDlgProc);
 CHECK(panel);
 const int ids[]={IDC_68K_STATUS_SR,IDC_68K_STATUS_ADR,IDC_68K_STATUS_DATA};
 const char *texts[]={"X=1 N=1 Z=1 V=1 C=1  SR=FFFF Cycles=4294967295",
  "A0=FFFFFFFF A1=FFFFFFFF A2=FFFFFFFF A3=FFFFFFFF\nA4=FFFFFFFF A5=FFFFFFFF A6=FFFFFFFF A7=FFFFFFFF",
  "D0=FFFFFFFF D1=FFFFFFFF D2=FFFFFFFF D3=FFFFFFFF\nD4=FFFFFFFF D5=FFFFFFFF D6=FFFFFFFF D7=FFFFFFFF"};
 RECT group,client,previous={0};
 GetWindowRect(GetDlgItem(panel,IDC_68K_USAGE_GROUP),&group);
 GetClientRect(panel,&client); MapWindowPoints(panel,NULL,(POINT*)&client,2);
 CHECK(group.right<=client.right && group.bottom<=client.bottom);
 for(unsigned i=0;i<3;i++) {
  HWND control=GetDlgItem(panel,ids[i]); RECT bounds,required={0},overlap;
  HDC dc=GetDC(control); HGDIOBJ old=SelectObject(dc,(HFONT)SendMessage(control,WM_GETFONT,0,0));
  DrawText(dc,texts[i],-1,&required,DT_CALCRECT|DT_LEFT|DT_NOPREFIX);
  SelectObject(dc,old); ReleaseDC(control,dc); GetWindowRect(control,&bounds);
  CHECK(bounds.right-bounds.left>=required.right && bounds.bottom-bounds.top>=required.bottom);
  CHECK(bounds.bottom<=client.bottom && !IntersectRect(&overlap,&bounds,&group));
  if(i)CHECK(bounds.top>=previous.bottom);
  previous=bounds;
 }
 Genesis_Started=1;
 for(unsigned i=0;i<256;i++) { main68k_context.odometer=500; M68K_Usage_Begin_Frame(); M68K_Usage_End_Frame(1000); }
 HWND graph=GetDlgItem(panel,IDC_68K_USAGE);
 HDC screen=GetDC(graph), dc=CreateCompatibleDC(screen);
 HBITMAP bitmap=CreateCompatibleBitmap(screen,300,140); HGDIOBJ old=SelectObject(dc,bitmap);
 DRAWITEMSTRUCT item={0}; item.hwndItem=graph; item.hDC=dc; SetRect(&item.rcItem,0,0,300,140);
 SendMessage(panel,WM_DRAWITEM,IDC_68K_USAGE,(LPARAM)&item);
 CHECK(GetPixel(dc,100,30)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,110)==RGB(0,0,255));
 Genesis_Started=0; SendMessage(panel,WM_DRAWITEM,IDC_68K_USAGE,(LPARAM)&item);
 CHECK(GetPixel(dc,100,110)==GetSysColor(COLOR_WINDOW));
 CHECK(M68K_Usage_GetHistory(values)==256); /* repaint never adds samples */
 SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(graph,screen);
 char dir[MAX_PATH], config[MAX_PATH]; RECT saved, restored; MSG msg;
 CHECK(GetTempPath(sizeof(dir),dir) && GetTempFileName(dir,"s68",0,config));
 m68kdebug_restore_window(config);
 CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_68K-1]);
 ShowWindow(panel,SW_SHOW); OpenedWindow_KMod[DMODE_68K-1]=TRUE;
 GetWindowRect(panel,&saved); m68kdebug_save_window(config); m68kdebug_destroy();
 CHECK(!HandleWindow_KMod[DMODE_68K-1]);
 CHECK(!PeekMessage(&msg,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
 m68kdebug_create(GetModuleHandle(NULL),NULL); m68kdebug_restore_window(config);
 panel=HandleWindow_KMod[DMODE_68K-1]; GetWindowRect(panel,&restored);
 CHECK(IsWindowVisible(panel) && OpenedWindow_KMod[DMODE_68K-1] && EqualRect(&saved,&restored));
 SendMessage(panel,WM_CLOSE,0,0);
 CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_68K-1]);
 m68kdebug_save_window(config); m68kdebug_destroy();
 m68kdebug_create(GetModuleHandle(NULL),NULL); m68kdebug_restore_window(config);
 panel=HandleWindow_KMod[DMODE_68K-1];
 CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_68K-1]);
 m68kdebug_destroy(); DeleteFile(config);
 puts("PASS: Genesis window: default closed, restores open/closed state and bounds, no WM_QUIT");
 puts("PASS: Genesis dialog: register bounds, graph placement, blue usage bars, inactive state");
 }


 /* Polling detection reads opcodes only, never the polled MMIO address. */
 for(int sub=0;sub<2;sub++) {
  memory=sub?Ram_Prg:Rom_Data;
  S68000CONTEXT *cpu=sub?&sub68k_context:&main68k_context;
  S68000CONTEXT c=*cpu;
  c.pc=0x200; c.sr=0x2704; memset(c.interrupts,0,sizeof(c.interrupts));
  c.areg[0]=sub?0x1000:0xff0100;
  if(sub)Ram_Prg[0x1001]=0; else Ram_68k[0x101]=0;
  word(0x200,0x4a10); word(0x202,0x67fc); /* TST.B (A0); BEQ wait */
  CHECK(M68K_Polling_Loop(&c)==0x201);
  c.pc=0x202; CHECK(M68K_Polling_Loop(&c)==0x201); c.pc=0x200;
  unsigned (*native)(int)=sub?sub68k_exec:main68k_exec;
  unsigned (*profiled)(int)=sub?S68K_Exec_Profiled:M68K_Exec_Profiled;
  void (*set)(void*)=sub?sub68k_SetContext:main68k_SetContext;
  unsigned (*trip)()=sub?sub68k_tripOdometer:main68k_tripOdometer;
  unsigned *idle=sub?&sub68k_idleCycles:&main68k_idleCycles;
  set(&c); trip(); *idle=0; unsigned nativeResult=native(1000);
  S68000CONTEXT expected=*cpu;
  set(&c); trip(); *idle=0; unsigned profiledResult=profiled(1000);
  CHECK(nativeResult==profiledResult && !memcmp(&expected,cpu,sizeof(expected)));
  CHECK(*idle>=990); /* Loop runs normally, but contributes almost no busy time. */
  /* A changed flag exits the wait; this slice must remain busy. */
  if(sub)Ram_Prg[0x1001]=1; else Ram_68k[0x101]=1;
  for(unsigned a=0x204;a<0x1000;a+=2)word(a,0x4e71);
  set(&c); trip(); *idle=0; profiled(1000); CHECK(*idle==0);
  /* Typical VDP bit wait, including absolute-long address extensions. */
  word(0x200,0x0839); word(0x202,3); word(0x204,0x00c0); word(0x206,5); word(0x208,0x67f6);
  CHECK(M68K_Polling_Loop(&c)==0x201);
  c.pc=0x204; CHECK(!M68K_Polling_Loop(&c)); c.pc=0x200;
  word(0x200,0x3039); word(0x202,0x00c0); word(0x204,4);
  word(0x206,0x0240); word(0x208,8); word(0x20a,0x67f4); /* MOVE.W status,D0; ANDI #8,D0; BEQ */
  CHECK(M68K_Polling_Loop(&c)==0x201);
  word(0x200,0x4a18); word(0x202,0x67fc); CHECK(!M68K_Polling_Loop(&c)); /* postincrement */
  word(0x200,0x5210); CHECK(!M68K_Polling_Loop(&c)); /* ADDQ memory */
  word(0x200,0x4a10); word(0x202,0x5240); word(0x204,0x67fa); CHECK(!M68K_Polling_Loop(&c)); /* useful counter */
  word(0x200,0x10bc); word(0x202,0); word(0x204,0x67fa); CHECK(!M68K_Polling_Loop(&c)); /* store */
  word(0x200,0x60fe); CHECK(M68K_Polling_Loop(&c)==0x201); /* BRA self */
  /* Fetch bounds and odd PCs are safe, unsupported code stays busy. */
  c.pc=0xffffff; CHECK(!M68K_Polling_Loop(&c));
 }
 puts("PASS: both 68K polling estimates: flag/VDP waits, spin, execution unchanged, exits/computation/stores/traversal excluded");
 /* Open ROM unloads the previous game through kmod_close. */
 m68kdebug_create(GetModuleHandle(NULL),NULL);
 s68kdebug_create(GetModuleHandle(NULL),NULL);
 HWND mainPanel=HandleWindow_KMod[DMODE_68K-1], subPanel=HandleWindow_KMod[DMODE_CD_68K-1];
 OpenWindow_KMod(DMODE_68K); OpenWindow_KMod(DMODE_CD_68K);
 RECT mainBefore,subBefore,mainAfter,subAfter;
 GetWindowRect(mainPanel,&mainBefore); GetWindowRect(subPanel,&subBefore);
 kmod_close();
 GetWindowRect(mainPanel,&mainAfter); GetWindowRect(subPanel,&subAfter);
 CHECK(IsWindowVisible(mainPanel) && IsWindowVisible(subPanel));
 CHECK(OpenedWindow_KMod[DMODE_68K-1] && OpenedWindow_KMod[DMODE_CD_68K-1]);
 CHECK(EqualRect(&mainBefore,&mainAfter) && EqualRect(&subBefore,&subAfter));
 CloseWindow_KMod(DMODE_68K); CloseWindow_KMod(DMODE_CD_68K);
 kmod_close();
 CHECK(!IsWindowVisible(mainPanel) && !IsWindowVisible(subPanel));
 CHECK(!OpenedWindow_KMod[DMODE_68K-1] && !OpenedWindow_KMod[DMODE_CD_68K-1]);
 m68kdebug_destroy(); s68kdebug_destroy();
 puts("PASS: ROM unload preserves both 68K panels' visibility and position; closed panels stay closed");
 puts("PASS: both CPUs: all 16 TRAP vectors, exception stacks, RTE and no RESET callback"); return 0;
}
