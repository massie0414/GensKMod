#ifndef _CPU_Z80_H
#define _CPU_Z80_H
#include "z80.h"

#ifdef __cplusplus
extern "C" {
#endif

#define Z80_USAGE_HISTORY 256
extern unsigned Z80_UsageBusyCycles;
void Z80_Usage_ClearHistory(void);
void Z80_Usage_Begin_Frame(void);
void Z80_Usage_End_Frame(unsigned budget);
unsigned Z80_Usage_GetHistory(unsigned long *values);
unsigned FASTCALL Z80_Exec_Profiled(Z80_CONTEXT *cpu, int target);
unsigned Z80_Polling_Loop(Z80_CONTEXT *cpu);
int Z80_Init(void);
void Z80_Reset(void);

#ifdef __cplusplus
};
#endif

#endif