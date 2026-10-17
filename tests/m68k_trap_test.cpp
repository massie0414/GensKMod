#include <stdio.h>
#include <string.h>
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Mem_M68k.h"
#include "../src/Gens/Mem_S68k.h"
#include "../src/Gens/Gens.h"

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
 puts("PASS: both CPUs: all 16 TRAP vectors, exception stacks, RTE and no RESET callback"); return 0;
}
