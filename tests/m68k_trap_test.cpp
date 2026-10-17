#include <stdio.h>
#include <string.h>
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Mem_M68k.h"
#include "../src/Gens/Gens.h"

static int resets;
static void reset_hook(void) { ++resets; }
static void word(unsigned a,unsigned v) { Rom_Data[a]=v&255; Rom_Data[a+1]=v>>8; }
static unsigned read_word(unsigned a) { return M68K_RW(a); }
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
 M68K_Init(); Init_Tab(); Rom_Size=0x10000; word(0,0x00ff); word(2,0xf000); word(4,0); word(6,0x200); M68K_Reset(0);
 for(unsigned n=0;n<16;n++) {
  memset(Rom_Data,0,65536);
  S68000CONTEXT c; main68k_GetContext(&c);
  c.resethandler=reset_hook; c.pc=0x200; c.areg[7]=0xfff000; c.sr=0x2700;
  unsigned handler=0x400+n*4;
  word(0x80+4*n,0); word(0x82+4*n,handler);
  word(0x200,0x4e40+n); word(handler,0x4e73);
  main68k_SetContext(&c); main68k_tripOdometer(); main68k_exec(1);
  CHECK(main68k_context.pc==handler); CHECK(resets==0);
  CHECK(main68k_context.areg[7]==0xffeffa);
  CHECK(read_word(0xffeffa)==0x2700 && read_word(0xffeffc)==0 && read_word(0xffeffe)==0x202);
  main68k_tripOdometer(); main68k_exec(1);
  CHECK(main68k_context.pc==0x202 && main68k_context.areg[7]==0xfff000);
 }
 puts("PASS: all 16 TRAP vectors, exception stacks, RTE and no RESET callback"); return 0;
}
