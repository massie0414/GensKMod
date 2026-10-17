#include "Star_68k.h"

#ifndef _CPU_68K_H
#define _CPU_68K_H

#ifdef __cplusplus
extern "C" {
#endif

extern struct S68000CONTEXT Context_68K;
	
unsigned M68K_Polling_Loop(struct S68000CONTEXT *cpu);
unsigned M68K_Exec_Profiled(int cycles);
unsigned S68K_Exec_Profiled(int cycles);
int M68K_Init();
#define M68K_USAGE_HISTORY 256
extern unsigned main68k_idleCycles;
void M68K_Usage_ClearHistory(void);
void M68K_Usage_Begin_Frame(void);
void M68K_Usage_End_Frame(unsigned budget);
unsigned M68K_Usage_GetHistory(unsigned long *values);

int S68K_Init();
#define S68K_USAGE_HISTORY 256
extern unsigned sub68k_idleCycles;
void S68K_Usage_ClearHistory(void);
void S68K_Usage_Begin_Frame(void);
void S68K_Usage_End_Frame(unsigned budget);
unsigned S68K_Usage_GetHistory(unsigned long *values);
void M68K_Reset(int System_ID);
void S68K_Reset();
void M68K_Reset_CPU();
void S68K_Reset_CPU();
void M68K_32X_Mode();
void M68K_Set_32X_Rom_Bank();
void M68K_Set_Prg_Ram();
void MS68K_Set_Word_Ram();

#ifdef __cplusplus
};
#endif

#endif