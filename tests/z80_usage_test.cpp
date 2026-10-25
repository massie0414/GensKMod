#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Mem_Z80.h"
#include "../src/Gens/Mem_M68k.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/resource.h"
#include "../src/Gens/kmod/z80.h"
extern "C" {
#include "../src/Gens/gens.h"
#include "../src/Gens/kmod/common.h"
}
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static void start(const unsigned char *code,unsigned size) {
 Z80_Reset(); memcpy(Ram_Z80,code,size); Z80_State=3; z80_Clear_Odo(&M_Z80); Z80_Usage_Begin_Frame();
}
int main() {
 Z80_Init(); unsigned long values[256];
 unsigned char nop[8192]={0}; start(nop,sizeof(nop)); z80_Exec(&M_Z80,1000);
 Z80_Usage_End_Frame(1000); CHECK(Z80_Usage_GetHistory(values)==1 && values[0]==1000);
 const unsigned char halt[]={0x76}; start(halt,sizeof(halt)); z80_Exec(&M_Z80,1000);
 Z80_Usage_End_Frame(1000); CHECK(Z80_Usage_GetHistory(values)==1 && values[0]==4);
 z80_Clear_Odo(&M_Z80); Z80_Usage_Begin_Frame(); z80_Exec(&M_Z80,1000);
 Z80_Usage_End_Frame(1000); CHECK(Z80_Usage_GetHistory(values)==2 && values[1]==0);
 Z80_Usage_Begin_Frame(); z80_Set_Odo(&M_Z80,2000); Z80_Usage_End_Frame(1000);
 CHECK(Z80_Usage_GetHistory(values)==3 && values[2]==0);
 for(unsigned i=0;i<300;i++) { Z80_Usage_Begin_Frame(); Z80_UsageBusyCycles=i; Z80_Usage_End_Frame(1000); }
 CHECK(Z80_Usage_GetHistory(values)==256 && values[0]==44 && values[255]==299);
 Z80_Reset(); CHECK(Z80_Usage_GetHistory(values)==0);
 const unsigned char poll[]={0x3a,0x00,0x01,0xb7,0x28,0xfa}; /* LD A,($100); OR A; JR Z,0 */
 start(poll,sizeof(poll)); Z80_CONTEXT initial=M_Z80;
 CHECK(Z80_Polling_Loop(&M_Z80)==1);
 z80_Exec(&M_Z80,1000); Z80_CONTEXT expected=M_Z80;
 M_Z80=initial; Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,1000);
 CHECK(!memcmp(&expected,&M_Z80,sizeof(expected)));
 /* First slice may change flags; a steady slice is recognized as idle. */
 Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,2000); CHECK(Z80_UsageBusyCycles==0);
 Ram_Z80[0x100]=1; Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,2100); CHECK(Z80_UsageBusyCycles>0);
 const unsigned char work[]={0x3c,0x20,0xfd}; start(work,sizeof(work)); CHECK(!Z80_Polling_Loop(&M_Z80));
 const unsigned char spin[]={0x18,0xfe}; start(spin,sizeof(spin)); CHECK(Z80_Polling_Loop(&M_Z80)==1);

 const unsigned char regPoll[]={0x46,0xcb,0x40,0x28,0xfb}; /* LD B,(HL); BIT 0,B; JR Z */
 start(regPoll,sizeof(regPoll)); M_Z80.HL.w.HL=0x100;
 CHECK(Z80_Polling_Loop(&M_Z80)==1);
 Z80_Exec_Profiled(&M_Z80,1000); Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,2000);
 CHECK(Z80_UsageBusyCycles==0);
 Ram_Z80[0x100]=1; Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,2100); CHECK(Z80_UsageBusyCycles>0);
 const unsigned char indexed[]={0xdd,0xcb,0,0x46,0x28,0xfa}; /* BIT 0,(IX); JR Z */
 start(indexed,sizeof(indexed)); M_Z80.IX.w.IX=0x100;
 CHECK(Z80_Polling_Loop(&M_Z80)==1);
 Z80_Exec_Profiled(&M_Z80,1000); Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,2000);
 CHECK(Z80_UsageBusyCycles==0);
 const unsigned char delays[][3]={{0x10,0xfe,0},{0x05,0x20,0xfd}};
 for(unsigned i=0;i<2;i++) {
  start(delays[i],3); M_Z80.BC.b.B=128; Z80_CONTEXT initialDelay=M_Z80;
  CHECK(Z80_Polling_Loop(&M_Z80)==1);
  z80_Exec(&M_Z80,100); Z80_CONTEXT expectedDelay=M_Z80;
  M_Z80=initialDelay; Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,100);
  CHECK(!memcmp(&expectedDelay,&M_Z80,sizeof(expectedDelay)) && Z80_UsageBusyCycles==0);
  /* The final slice exits the delay, and subsequent work remains busy. */
  Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,4000); CHECK(Z80_UsageBusyCycles>0);
 }
 const unsigned char writeLoop[]={0x77,0xb7,0x28,0xfc}; start(writeLoop,sizeof(writeLoop)); CHECK(!Z80_Polling_Loop(&M_Z80));
 const unsigned char decMemory[]={0x35,0x20,0xfd}; start(decMemory,sizeof(decMemory)); CHECK(!Z80_Polling_Loop(&M_Z80));
 const unsigned char increment[]={0x23,0x7e,0xb7,0x28,0xfb}; start(increment,sizeof(increment)); CHECK(!Z80_Polling_Loop(&M_Z80));
 const unsigned char bitWrite[]={0xcb,0x86,0x28,0xfc}; start(bitWrite,sizeof(bitWrite)); CHECK(!Z80_Polling_Loop(&M_Z80));

 const unsigned char nopSpin[]={0,0xc3,0,0}; start(nopSpin,sizeof(nopSpin));
 CHECK(Z80_Polling_Loop(&M_Z80)==1); Z80_Exec_Profiled(&M_Z80,1000); CHECK(Z80_UsageBusyCycles==0);
 const unsigned char stackDelay[]={0xe5,0xe1,0x0b,0x79,0xb7,0xc2,0,0};
 /* The actual Super Thunder Blade delay shape, relocated to zero. */
 for(unsigned phase=0;phase<6;phase++) {
  start(stackDelay,sizeof(stackDelay)); M_Z80.BC.w.BC=0x1e0; M_Z80.HL.w.HL=0x1234;
  M_Z80.SP.w.SP=0x1ff0; z80_Set_AF(&M_Z80,0xe000); z80_Set_PC(&M_Z80,phase);
  if(phase==1) { M_Z80.SP.w.SP-=2; Ram_Z80[0x1fee]=0x34; Ram_Z80[0x1fef]=0x12; }
  Z80_CONTEXT beforeDelay=M_Z80;
  CHECK(Z80_Polling_Loop(&M_Z80)==1);
  z80_Exec(&M_Z80,200); Z80_CONTEXT afterDelay=M_Z80;
  unsigned char expectedStack[4]; memcpy(expectedStack,Ram_Z80+0x1fec,4);
  M_Z80=beforeDelay; Z80_Usage_Begin_Frame(); Z80_Exec_Profiled(&M_Z80,200);
  CHECK(!memcmp(&afterDelay,&M_Z80,sizeof(afterDelay)) && Z80_UsageBusyCycles==0);
  CHECK(!memcmp(expectedStack,Ram_Z80+0x1fec,4));
 }
 const unsigned char usefulPush[]={0xe5,0xe1,0x0b,0x79,0xb7,0x77,0xc2,0,0};
 start(usefulPush,sizeof(usefulPush)); CHECK(!Z80_Polling_Loop(&M_Z80));
 /* MD_FM's complete scanline-debt poll, with its dispatch exit at $40. */
 const unsigned char clockWait[]={0x3a,0x08,0x7f,0xb8,0x28,0x0d,0x57,0x90,0x42,
  0xfe,0xfa,0x38,0x02,0xd6,0xfa,0x57,0x79,0x92,0x4f,0x79,0xb7,
  0xfa,0x40,0,0xca,0x40,0,0x18,0xe3};
 const unsigned phases[]={0,3,4,6,7,8,9,11,13,15,16,17,18,19,20,21,24,27};
 for(unsigned i=0;i<sizeof(phases)/sizeof(phases[0]);++i) {
  start(clockWait,sizeof(clockWait)); M_Z80.BC.w.BC=100; M_Z80.DE.w.DE=0;
  z80_Set_AF(&M_Z80,(phases[i]==17 || phases[i]==18 || phases[i]>=20)?0x6400:0);
  z80_Set_PC(&M_Z80,phases[i]); Z80_CONTEXT beforeClock=M_Z80;
  unsigned clockResult=z80_Exec(&M_Z80,200); Z80_CONTEXT afterClock=M_Z80;
  M_Z80=beforeClock; Z80_Usage_Begin_Frame(); CHECK(Z80_Exec_Profiled(&M_Z80,200)==clockResult);
  /* CycleTD/temporary fields describe the last host call, not Z80 state. */
  CHECK(!memcmp(&afterClock,&M_Z80,offsetof(Z80_CONTEXT,TmpSav0)));
  if(afterClock.CycleCnt!=M_Z80.CycleCnt || Z80_UsageBusyCycles)
   printf("clock phase=%u cycles=%u/%u busy=%u status=%u\n",phases[i],afterClock.CycleCnt,M_Z80.CycleCnt,Z80_UsageBusyCycles,M_Z80.Status);
  CHECK(afterClock.CycleCnt==M_Z80.CycleCnt && Z80_UsageBusyCycles==0);
 }
 start(clockWait,sizeof(clockWait)); M_Z80.BC.w.BC=0;
 const unsigned char clockWork[]={0x3e,0x55,0x32,0,1,0xc3,0x40,0};
 memcpy(Ram_Z80+0x40,clockWork,sizeof(clockWork));
 Z80_Exec_Profiled(&M_Z80,1000);
 CHECK(Ram_Z80[0x100]==0x55 && Z80_UsageBusyCycles>0);
 start(clockWait,sizeof(clockWait)); Ram_Z80[6]=0x77; /* Memory write: not a wait. */
 M_Z80.HL.w.HL=0x100; M_Z80.BC.w.BC=100; Z80_Exec_Profiled(&M_Z80,200);
 CHECK(Z80_UsageBusyCycles>0);
 /* CD32X_Template clock at $00e0: RET M / RET Z, including both exits. */
 const unsigned char returnWait[]={0x3a,0x08,0x7f,0xb8,0x28,0x0d,0x57,0x90,0x42,
  0xfe,0xfa,0x38,0x02,0xd6,0xfa,0x57,0x79,0x92,0x4f,0x79,0xb7,
  0xf8,0xc8,0x18,0xe7};
 const unsigned returnPhases[]={0,3,4,6,7,8,9,11,13,15,16,17,18,19,20,21,22,23};
 for(unsigned i=0;i<sizeof(returnPhases)/sizeof(returnPhases[0]);++i) {
  start(returnWait,sizeof(returnWait)); M_Z80.BC.w.BC=100;
  z80_Set_AF(&M_Z80,returnPhases[i]>=17?0x6400:0);
  z80_Set_PC(&M_Z80,returnPhases[i]); Z80_CONTEXT beforeReturn=M_Z80;
  unsigned nativeResult=z80_Exec(&M_Z80,200); Z80_CONTEXT afterReturn=M_Z80;
  M_Z80=beforeReturn; Z80_Usage_Begin_Frame();
  CHECK(Z80_Exec_Profiled(&M_Z80,200)==nativeResult);
  CHECK(!memcmp(&afterReturn,&M_Z80,offsetof(Z80_CONTEXT,TmpSav0)));
  CHECK(afterReturn.CycleCnt==M_Z80.CycleCnt && Z80_UsageBusyCycles==0);
 }
 for(unsigned debt=0;debt<2;++debt) {
  start(returnWait,sizeof(returnWait)); M_Z80.BC.w.BC=debt?0xff:0;
  M_Z80.SP.w.SP=0x1fee; Ram_Z80[0x1fee]=0x40; Ram_Z80[0x1fef]=0;
  memcpy(Ram_Z80+0x40,clockWork,sizeof(clockWork));
  Z80_CONTEXT beforeExit=M_Z80;
  unsigned nativeResult=z80_Exec(&M_Z80,1000); Z80_CONTEXT afterExit=M_Z80;
  M_Z80=beforeExit; Ram_Z80[0x100]=0; Z80_Usage_Begin_Frame();
  CHECK(Z80_Exec_Profiled(&M_Z80,1000)==nativeResult);
  CHECK(!memcmp(&afterExit,&M_Z80,offsetof(Z80_CONTEXT,TmpSav0)));
  CHECK(afterExit.CycleCnt==M_Z80.CycleCnt);
  CHECK(Ram_Z80[0x100]==0x55 && Z80_UsageBusyCycles>0);
 }
 start(returnWait,sizeof(returnWait)); Ram_Z80[6]=0x77;
 M_Z80.HL.w.HL=0x100; M_Z80.BC.w.BC=100; Z80_Exec_Profiled(&M_Z80,200);
 CHECK(Z80_UsageBusyCycles>0);
 puts("PASS: CD32X RET clock waits: all boundaries, both exits, unchanged execution, writes counted");
 puts("PASS: extended Z80 waits: register/indexed bit polling and countdown delays; writes/traversal excluded");
 puts("PASS: Z80 usage: busy, HALT entry/halted, bus-held, ring rollover/reset, polling, unchanged execution, wait exit");
 z80debug_create(GetModuleHandle(NULL),NULL); HWND panel=HandleWindow_KMod[DMODE_Z80-1]; CHECK(panel);
 const int ids[]={IDC_Z80_STATUS_RS1,IDC_Z80_STATUS_RS3,IDC_Z80_STATUS_MISC,IDC_Z80_STATUS_FLAG,IDC_Z80_STATUS_BANK};
 const char *texts[]={"AF =FFFF BC =FFFF DE =FFFF HL =FFFF\nAF2=FFFF BC2=FFFF DE2=FFFF HL2=FFFF",
 "IX =FFFF IY =FFFF SP =FFFF", "IFF1=1 IFF2=1 I=FF R=FF IM=02", "S=1 Z=1 Y=1 H=1 X=1 P=1 N=1 C=1", "Bank = FFFFFFFF State=03"};
 RECT group,client,previous={0}; GetWindowRect(GetDlgItem(panel,IDC_Z80_USAGE_GROUP),&group);
 GetClientRect(panel,&client); MapWindowPoints(panel,NULL,(POINT*)&client,2);
 CHECK(group.right<=client.right && group.bottom<=client.bottom);
 for(unsigned i=0;i<5;i++) {
  HWND control=GetDlgItem(panel,ids[i]); RECT bounds,required={0},overlap;
  HDC dc=GetDC(control); HGDIOBJ old=SelectObject(dc,(HFONT)SendMessage(control,WM_GETFONT,0,0));
  DrawText(dc,texts[i],-1,&required,DT_CALCRECT|DT_LEFT|DT_NOPREFIX);
  SelectObject(dc,old); ReleaseDC(control,dc); GetWindowRect(control,&bounds);
  CHECK(bounds.right-bounds.left>=required.right && bounds.bottom-bounds.top>=required.bottom);
  CHECK(bounds.bottom<=client.bottom && !IntersectRect(&overlap,&bounds,&group));
  if(i)CHECK(bounds.top>=previous.bottom); previous=bounds;
 }
 Genesis_Started=1;
 for(unsigned i=0;i<256;i++) { Z80_UsageBusyCycles=500; Z80_Usage_End_Frame(1000); }
 HWND graph=GetDlgItem(panel,IDC_Z80_USAGE);
 HDC screen=GetDC(graph), dc=CreateCompatibleDC(screen); HBITMAP bitmap=CreateCompatibleBitmap(screen,300,140);
 HGDIOBJ old=SelectObject(dc,bitmap); DRAWITEMSTRUCT item={0}; item.hwndItem=graph; item.hDC=dc; SetRect(&item.rcItem,0,0,300,140);
 SendMessage(panel,WM_DRAWITEM,IDC_Z80_USAGE,(LPARAM)&item);
 CHECK(GetPixel(dc,100,30)==GetSysColor(COLOR_WINDOW) && GetPixel(dc,100,110)==RGB(0,0,255));
 Genesis_Started=0; SendMessage(panel,WM_DRAWITEM,IDC_Z80_USAGE,(LPARAM)&item);
 CHECK(GetPixel(dc,100,110)==GetSysColor(COLOR_WINDOW));
 SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc); ReleaseDC(graph,screen);
 char dir[MAX_PATH],config[MAX_PATH]; RECT saved,restored; MSG msg;
 CHECK(GetTempPath(sizeof(dir),dir) && GetTempFileName(dir,"z80",0,config));
 z80debug_restore_window(config); CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_Z80-1]);
 OpenWindow_KMod(DMODE_Z80); GetWindowRect(panel,&saved); kmod_close(); GetWindowRect(panel,&restored);
 CHECK(IsWindowVisible(panel) && OpenedWindow_KMod[DMODE_Z80-1] && EqualRect(&saved,&restored));
 z80debug_save_window(config); z80debug_destroy(); CHECK(!HandleWindow_KMod[DMODE_Z80-1]);
 CHECK(!PeekMessage(&msg,NULL,WM_QUIT,WM_QUIT,PM_REMOVE));
 z80debug_create(GetModuleHandle(NULL),NULL); z80debug_restore_window(config); panel=HandleWindow_KMod[DMODE_Z80-1];
 GetWindowRect(panel,&restored); CHECK(IsWindowVisible(panel) && OpenedWindow_KMod[DMODE_Z80-1] && EqualRect(&saved,&restored));
 SendMessage(panel,WM_CLOSE,0,0); kmod_close(); CHECK(!IsWindowVisible(panel) && !OpenedWindow_KMod[DMODE_Z80-1]);
 z80debug_save_window(config); z80debug_destroy(); z80debug_create(GetModuleHandle(NULL),NULL); z80debug_restore_window(config);
 CHECK(!IsWindowVisible(HandleWindow_KMod[DMODE_Z80-1]) && !OpenedWindow_KMod[DMODE_Z80-1]);
 z80debug_destroy(); DeleteFile(config);
 puts("PASS: Z80 panel: text/graph bounds, blue bars/inactive, open/closed/position restore, ROM switching");
 return 0;
}
