#include <stdio.h>
#include <windows.h>
#include "Cpu_68k.h"
#include "mem_M68K.h"
#include "mem_S68K.h"
#include "word_ram_transfer.h"
#include "main_word_ram_transfer.h"
#include "vram_transfer.h"
#include "genesis_ram_transfer.h"
#include "program_ram_transfer.h"
#include "mem_SH2.h"
#include "save.h"
#include "ym2612.h"
#include "misc.h"
#include "gfx_cd.h"

#include "io.h"
extern int SegaCD_Started;
#include "cd_sys.h"


#define GENESIS 0
#define _32X    1
#define SEGACD  2

/*** global variables ***/

struct S68000CONTEXT Context_68K;

struct STARSCREAM_PROGRAMREGION M68K_Fetch[] =
{
	{0x000000, 0x3FFFFF, (unsigned)0x000000},
	{0xFF0000, 0xFFFFFF, (unsigned)&Ram_68k[0] - 0xFF0000},
	{0xF00000, 0xF0FFFF, (unsigned)&Ram_68k[0] - 0xF00000},
	{0xEF0000, 0xEFFFFF, (unsigned)&Ram_68k[0] - 0xEF0000},
	{-1, -1, (unsigned) NULL},
	{-1, -1, (unsigned) NULL},
	{-1, -1, (unsigned) NULL}
};
		
struct STARSCREAM_DATAREGION M68K_Read_Byte[5] =
{
	{0x000000, 0x3FFFFF, (void *)M68K_Data_RB, NULL},
	{0xFF0000, 0xFFFFFF, NULL, &Ram_68k[0]},
	{0x400000, 0xFEFFFF, (void *)M68K_Data_RB, NULL},
	{-1, -1, NULL, NULL}
};

struct STARSCREAM_DATAREGION M68K_Read_Word[5] =
{
	{0x000000, 0x3FFFFF, (void *)M68K_Data_RW, NULL},
	{0xFF0000, 0xFFFFFF, NULL, &Ram_68k[0]},
	{0x400000, 0xFEFFFF, (void *)M68K_Data_RW, NULL},
	{-1, -1, NULL, NULL}
};

struct STARSCREAM_DATAREGION M68K_Write_Byte[] =
{
	{0xFF0000, 0xFFFFFF, NULL, &Ram_68k[0]},
	{0x000000, 0xFEFFFF, (void *)M68K_Data_WB, NULL},
	{-1, -1, NULL, NULL}
};

struct STARSCREAM_DATAREGION M68K_Write_Word[] =
{
	{0xFF0000, 0xFFFFFF, NULL, &Ram_68k[0]},
	{0x000000, 0xFEFFFF, (void *)M68K_Data_WW, NULL},
	{-1, -1, NULL, NULL}
};


struct STARSCREAM_PROGRAMREGION S68K_Fetch[] =
{
	{0x000000, 0x07FFFF, (unsigned)&Ram_Prg[0]},
	{-1, -1, (unsigned) NULL},
	{-1, -1, (unsigned) NULL}
};
		
struct STARSCREAM_DATAREGION S68K_Read_Byte[] =
{
	{0x000000, 0xFFFFFF, (void *)S68K_Data_RB, NULL},
	{-1, -1, NULL, NULL}
};

struct STARSCREAM_DATAREGION S68K_Read_Word[] =
{
	{0x000000, 0xFFFFFF, (void *)S68K_Data_RW, NULL},
	{-1, -1, NULL, NULL}
};

struct STARSCREAM_DATAREGION S68K_Write_Byte[] =
{
	{0x000000, 0xFFFFFF, (void *)S68K_Data_WB, NULL},
	{-1, -1, NULL, NULL}
};

struct STARSCREAM_DATAREGION S68K_Write_Word[] =
{
	{0x000000, 0xFFFFFF, (void *)S68K_Data_WW, NULL},
	{-1, -1, NULL, NULL}
};


void M68K_Reset_Handler()
{
//	Init_Memory_M68K(GENESIS);
}


void S68K_Reset_Handler()
{
//	Init_Memory_M68K(SEGACD);
}


/*** M68K_Init - initialise the main 68K ***/

int M68K_Init(void)
{
	memset(&Context_68K, 0, sizeof(Context_68K));

	Context_68K.s_fetch = Context_68K.u_fetch = Context_68K.fetch = M68K_Fetch;
	Context_68K.s_readbyte = Context_68K.u_readbyte = Context_68K.readbyte = M68K_Read_Byte;
	Context_68K.s_readword = Context_68K.u_readword = Context_68K.readword = M68K_Read_Word;
	Context_68K.s_writebyte = Context_68K.u_writebyte = Context_68K.writebyte = M68K_Write_Byte;
	Context_68K.s_writeword = Context_68K.u_writeword = Context_68K.writeword = M68K_Write_Word;
	Context_68K.resethandler = (void *)M68K_Reset_Handler;

	main68k_SetContext(&Context_68K);
	main68k_init();

	return 1;
}


/*** S68K_Init - initialise the sub 68K ***/

int S68K_Init(void)
{
	memset(&Context_68K, 0, sizeof(Context_68K));

	Context_68K.s_fetch = Context_68K.u_fetch = Context_68K.fetch = S68K_Fetch;
	Context_68K.s_readbyte = Context_68K.u_readbyte = Context_68K.readbyte = S68K_Read_Byte;
	Context_68K.s_readword = Context_68K.u_readword = Context_68K.readword = S68K_Read_Word;
	Context_68K.s_writebyte = Context_68K.u_writebyte = Context_68K.writebyte = S68K_Write_Byte;
	Context_68K.s_writeword = Context_68K.u_writeword = Context_68K.writeword = S68K_Write_Word;
	Context_68K.resethandler = (void *)S68K_Reset_Handler;

	sub68k_SetContext(&Context_68K);
	sub68k_init();

	return 1;
}


/*** M68K_Reset - general reset of the main 68K CPU ***/

void M68K_Reset(int System_ID)
{
	M68K_Usage_ClearHistory();
	memset(Ram_68k, 0, 64 * 1024);

	M68K_Fetch[0].lowaddr = 0x000000;
	M68K_Fetch[0].highaddr = Rom_Size - 1;
	M68K_Fetch[0].offset = (unsigned) &Rom_Data[0] - 0x000000;

	M68K_Fetch[1].lowaddr = 0xFF0000;
	M68K_Fetch[1].highaddr = 0xFFFFFF;
	M68K_Fetch[1].offset = (unsigned)&Ram_68k[0] - 0xFF0000;

	if (System_ID == GENESIS)
	{
		M68K_Fetch[2].lowaddr = 0xF00000;
		M68K_Fetch[2].highaddr = 0xF0FFFF;
		M68K_Fetch[2].offset = (unsigned)&Ram_68k[0] - 0xF00000;
		
		M68K_Fetch[3].lowaddr = 0xEF0000;
		M68K_Fetch[3].highaddr = 0xEFFFFF;
		M68K_Fetch[3].offset = (unsigned)&Ram_68k[0] - 0xEF0000;

		M68K_Fetch[4].lowaddr = -1;
		M68K_Fetch[4].highaddr = -1;
		M68K_Fetch[4].offset = (unsigned) NULL;


	}
	else if (System_ID == _32X)
	{
		Bank_SH2 = 0;

		M68K_Fetch[2].lowaddr = 0xF00000;
		M68K_Fetch[2].highaddr = 0xF0FFFF;
		M68K_Fetch[2].offset = (unsigned)&Ram_68k[0] - 0xF00000;
		
		M68K_Fetch[3].lowaddr = 0xEF0000;
		M68K_Fetch[3].highaddr = 0xEFFFFF;
		M68K_Fetch[3].offset = (unsigned)&Ram_68k[0] - 0xEF0000;

		M68K_Fetch[4].lowaddr = -1;
		M68K_Fetch[4].highaddr = -1;
		M68K_Fetch[4].offset = (unsigned) NULL;
	}
	else if (System_ID == SEGACD)
	{
		Bank_M68K = 0;

		MS68K_Set_Word_Ram();

		M68K_Fetch[3].lowaddr = 0x020000;
		M68K_Fetch[3].highaddr = 0x03FFFF;
		M68K_Set_Prg_Ram();

		M68K_Fetch[4].lowaddr = 0xF00000;
		M68K_Fetch[4].highaddr = 0xF0FFFF;
		M68K_Fetch[4].offset = (unsigned)&Ram_68k[0] - 0xF00000;
		
		M68K_Fetch[5].lowaddr = 0xEF0000;
		M68K_Fetch[5].highaddr = 0xEFFFFF;
		M68K_Fetch[5].offset = (unsigned)&Ram_68k[0] - 0xEF0000;

		M68K_Fetch[6].lowaddr = -1;
		M68K_Fetch[6].highaddr = -1;
		M68K_Fetch[6].offset = (unsigned) NULL;
	}

	main68k_reset();

	Init_Memory_M68K(System_ID);
}


/*** S68K_Reset - general reset of the sub 68K CPU ***/

void S68K_Reset(void)
{
	S68K_Usage_ClearHistory();
	memset(Ram_Prg, 0, 512 * 1024);
	memset(Ram_Word_2M, 0, 256 * 1024);
	memset(Ram_Word_1M, 0, 256 * 1024);

	memset(COMM.Command, 0, 8 * 5);
	memset(COMM.Status, 0, 8 * 5);

	LED_Status = S68K_State = S68K_Mem_WP = S68K_Mem_PM = Ram_Word_State = 0;
	COMM.Flag = Init_Timer_INT3 = Timer_INT3 = Int_Mask_S68K = 0;
	Font_COLOR = Font_BITS = 0;

	MS68K_Set_Word_Ram();

	sub68k_reset();
}




/***   M68K_32X_Mode - modify 32x mode    ***
 *** - Called only during 32X emulation - ***/

void M68K_32X_Mode()
{
    if (SegaCD_Started) return; // CD boot keeps its BIOS and RAM map.
//	if (_32X_ADEN && !_32X_RV)			// 32X ON
	if (_32X_ADEN)			// 32X ON
	{
		if (!_32X_RV)		// ROM MOVED
		{
			M68K_Fetch[0].lowaddr = 0x880000;
			M68K_Fetch[0].highaddr = 0x8FFFFF;
			M68K_Fetch[0].offset = (unsigned) &Rom_Data[0] - 0x880000;

			M68K_Fetch[1].lowaddr = 0x900000;
			M68K_Fetch[1].highaddr = 0x9FFFFF;
			M68K_Set_32X_Rom_Bank();

			M68K_Fetch[2].lowaddr = 0xFF0000;
			M68K_Fetch[2].highaddr = 0xFFFFFF;
			M68K_Fetch[2].offset = (unsigned) &Ram_68k[0] - 0xFF0000;

			M68K_Fetch[3].lowaddr = 0x00;
			M68K_Fetch[3].highaddr = 0xFF;
			M68K_Fetch[3].offset = (unsigned)&_32X_Genesis_Rom[0] - 0x000000;

			M68K_Fetch[4].lowaddr = 0xEF0000;
			M68K_Fetch[4].highaddr = 0xEFFFFF;
			M68K_Fetch[4].offset = (unsigned) &Ram_68k[0] - 0xEF0000;

			M68K_Fetch[5].lowaddr = 0xF00000;
			M68K_Fetch[5].highaddr = 0xF0FFFF;
			M68K_Fetch[5].offset = (unsigned) &Ram_68k[0] - 0xF00000;
		
			M68K_Fetch[6].lowaddr = -1;
			M68K_Fetch[6].highaddr = -1;
			M68K_Fetch[6].offset = (unsigned) NULL;

			M68K_Read_Byte_Table[0] = _32X_M68K_Read_Byte_Table[4 * 2];
			M68K_Read_Word_Table[0] = _32X_M68K_Read_Word_Table[4 * 2];
		}
		else		// ROM NOT MOVED BUT BIOS PRESENT
		{
			M68K_Fetch[0].lowaddr = 0x000100;
			M68K_Fetch[0].highaddr = Rom_Size - 1;
			M68K_Fetch[0].offset = (unsigned) &Rom_Data[0] - 0x000000;

			M68K_Fetch[1].lowaddr = 0xFF0000;
			M68K_Fetch[1].highaddr = 0xFFFFFF;
			M68K_Fetch[1].offset = (unsigned) &Ram_68k[0] - 0xFF0000;

			M68K_Fetch[2].lowaddr = 0x00;
			M68K_Fetch[2].highaddr = 0xFF;
			M68K_Fetch[2].offset = (unsigned)&_32X_Genesis_Rom[0] - 0x000000;

			M68K_Fetch[3].lowaddr = 0xF00000;
			M68K_Fetch[3].highaddr = 0xF0FFFF;
			M68K_Fetch[3].offset = (unsigned) &Ram_68k[0] - 0xF00000;
		
			M68K_Fetch[4].lowaddr = 0xEF0000;
			M68K_Fetch[4].highaddr = 0xEFFFFF;
			M68K_Fetch[4].offset = (unsigned) &Ram_68k[0] - 0xEF0000;

			M68K_Fetch[5].lowaddr = -1;
			M68K_Fetch[5].highaddr = -1;
			M68K_Fetch[5].offset = (unsigned) NULL;

			M68K_Read_Byte_Table[0] = _32X_M68K_Read_Byte_Table[4 * 2 + 1];
			M68K_Read_Word_Table[0] = _32X_M68K_Read_Word_Table[4 * 2 + 1];
		}
	}
	else
	{
		M68K_Fetch[0].lowaddr = 0x000000;
		M68K_Fetch[0].highaddr = Rom_Size - 1;
		M68K_Fetch[0].offset = (unsigned) &Rom_Data[0] - 0x000000;

		M68K_Fetch[1].lowaddr = 0xFF0000;
		M68K_Fetch[1].highaddr = 0xFFFFFF;
		M68K_Fetch[1].offset = (unsigned) &Ram_68k[0] - 0xFF0000;

		M68K_Fetch[2].lowaddr = 0xF00000;
		M68K_Fetch[2].highaddr = 0xF0FFFF;
		M68K_Fetch[2].offset = (unsigned) &Ram_68k[0] - 0xF00000;
		
		M68K_Fetch[3].lowaddr = 0xEF0000;
		M68K_Fetch[3].highaddr = 0xEFFFFF;
		M68K_Fetch[3].offset = (unsigned) &Ram_68k[0] - 0xEF0000;

		M68K_Fetch[4].lowaddr = -1;
		M68K_Fetch[4].highaddr = -1;
		M68K_Fetch[4].offset = (unsigned) NULL;

		M68K_Read_Byte_Table[0] = _32X_M68K_Read_Byte_Table[0];
		M68K_Read_Word_Table[0] = _32X_M68K_Read_Word_Table[0];
	}
}


/*** M68K_Set_32X_Rom_Bank - modify 32x rom bank ***
 ***     - Called only during 32X emulation -    ***/

void M68K_Set_32X_Rom_Bank()
{
    if (SegaCD_Started) return;
	if (_32X_ADEN && !_32X_RV)
	{
		M68K_Fetch[1].offset = (unsigned) &Rom_Data[Bank_SH2 << 20] - 0x900000;

		M68K_Read_Byte_Table[(9 * 2) + 0] = _32X_M68K_Read_Byte_Table[(Bank_SH2 << 1) + 0];
		M68K_Read_Byte_Table[(9 * 2) + 1] = _32X_M68K_Read_Byte_Table[(Bank_SH2 << 1) + 1];
		M68K_Read_Word_Table[(9 * 2) + 0] = _32X_M68K_Read_Word_Table[(Bank_SH2 << 1) + 0];
		M68K_Read_Word_Table[(9 * 2) + 1] = _32X_M68K_Read_Word_Table[(Bank_SH2 << 1) + 1];
	}
	else
	{


	}
}


/*** M68K_Set_Prg_Ram - modify bank Prg_Ram fetch ***
 ***   - Called only during SEGA CD emulation -   ***/

void M68K_Set_Prg_Ram()
{
	M68K_Fetch[3].offset = (unsigned) &Ram_Prg[Bank_M68K] - 0x020000;
}


/*** MS68K_Set_Word_Ram - modify bank Word_Ram fetch ***
 ***    - Called only during SEGA CD emulation -     ***/

void MS68K_Set_Word_Ram(void)
{
	switch(Ram_Word_State)
	{
		case 0:		// Mode 2M -> Assigned to Main CPU
			M68K_Fetch[2].lowaddr = 0x200000;
			M68K_Fetch[2].highaddr = 0x23FFFF;
			M68K_Fetch[2].offset = (unsigned) &Ram_Word_2M[0] - 0x200000;

//			S68K_Fetch[1].lowaddr = -1;
//			S68K_Fetch[1].highaddr = -1;		
//			S68K_Fetch[1].offset = (unsigned) NULL;

			S68K_Fetch[1].lowaddr = 0x080000;		// why not after all...
			S68K_Fetch[1].highaddr = 0x0BFFFF;		
			S68K_Fetch[1].offset = (unsigned) &Ram_Word_2M[0] - 0x080000;
			break;

		case 1:		// Mode 2M -> Assigned to Sub CPU
//			M68K_Fetch[2].lowaddr = -1;
//			M68K_Fetch[2].highaddr = -1;
//			M68K_Fetch[2].offset = (unsigned) NULL;

			M68K_Fetch[2].lowaddr = 0x200000;		// why not after all...
			M68K_Fetch[2].highaddr = 0x23FFFF;
			M68K_Fetch[2].offset = (unsigned) &Ram_Word_2M[0] - 0x200000;

			S68K_Fetch[1].lowaddr = 0x080000;
			S68K_Fetch[1].highaddr = 0x0BFFFF;		
			S68K_Fetch[1].offset = (unsigned) &Ram_Word_2M[0] - 0x080000;
			break;

		case 2:		// Mode 1M -> Bank 0 to Main CPU
			M68K_Fetch[2].lowaddr = 0x200000;			// Bank 0
			M68K_Fetch[2].highaddr = 0x21FFFF;
			M68K_Fetch[2].offset = (unsigned) &Ram_Word_1M[0] - 0x200000;

			S68K_Fetch[1].lowaddr = 0x0C0000;			// Bank 1
			S68K_Fetch[1].highaddr = 0x0DFFFF;		
			S68K_Fetch[1].offset = (unsigned) &Ram_Word_1M[0x20000] - 0x0C0000;
			break;

		case 3:		// Mode 1M -> Bank 0 to Sub CPU
			M68K_Fetch[2].lowaddr = 0x200000;			// Bank 1
			M68K_Fetch[2].highaddr = 0x21FFFF;
			M68K_Fetch[2].offset = (unsigned) &Ram_Word_1M[0x20000] - 0x200000;

			S68K_Fetch[1].lowaddr = 0x0C0000;			// Bank 0
			S68K_Fetch[1].highaddr = 0x0DFFFF;		
			S68K_Fetch[1].offset = (unsigned) &Ram_Word_1M[0] - 0x0C0000;
			break;
	}
}


/*** M68K_Reset_CPU - just reset the main 68K cpu ***/

void M68K_Reset_CPU()
{
	M68K_Usage_ClearHistory();
	main68k_reset();
}


/*** S68K_Reset_CPU - just reset the sub 68K cpu ***/

void S68K_Reset_CPU()
{
	S68K_Usage_ClearHistory();
	sub68k_reset();
}

/* Complete emulated frames only. STOP cycles and time held in reset/bus
 * request and recognized polling loops are idle. No host timing. */
static struct {
    unsigned long values[S68K_USAGE_HISTORY];
    unsigned next, count;
} s68kUsage;
void S68K_Usage_ClearHistory(void)
{
    WordRAMTransfer_Reset();
    ProgramRAMTransfer_Reset();
    memset(&s68kUsage, 0, sizeof(s68kUsage));
    sub68k_idleCycles = 0;
}
void S68K_Usage_Begin_Frame(void)
{
    sub68k_idleCycles = 0;
}
void S68K_Usage_End_Frame(unsigned budget)
{
    unsigned total = sub68k_readOdometer();
    unsigned busy = total > sub68k_idleCycles ? total - sub68k_idleCycles : 0;
    if (!budget) return;
    if (busy > budget) busy = budget;
    s68kUsage.values[s68kUsage.next] = (unsigned long)((double)busy * 1000.0 / budget + 0.5);
    s68kUsage.next = (s68kUsage.next + 1) % S68K_USAGE_HISTORY;
    if (s68kUsage.count < S68K_USAGE_HISTORY) ++s68kUsage.count;
}
unsigned S68K_Usage_GetHistory(unsigned long *values)
{
    unsigned i;
    for (i = 0; i < s68kUsage.count; ++i)
        values[i] = s68kUsage.values[(s68kUsage.next + S68K_USAGE_HISTORY - s68kUsage.count + i) % S68K_USAGE_HISTORY];
    return s68kUsage.count;
}
/* Complete emulated frames only. STOP cycles and time held in reset/bus
 * request and recognized polling loops are idle. No host timing. */
static struct {
    unsigned long values[M68K_USAGE_HISTORY];
    unsigned next, count;
} m68kUsage;
void M68K_Usage_ClearHistory(void)
{
    MainWordRAMTransfer_Reset();
    VRAMTransfer_Reset();
    GenesisRAMTransfer_Reset();
    memset(&m68kUsage, 0, sizeof(m68kUsage));
    main68k_idleCycles = 0;
}
void M68K_Usage_Begin_Frame(void)
{
    main68k_idleCycles = 0;
}
void M68K_Usage_End_Frame(unsigned budget)
{
    unsigned total = main68k_readOdometer();
    unsigned busy = total > main68k_idleCycles ? total - main68k_idleCycles : 0;
    if (!budget) return;
    if (busy > budget) busy = budget;
    m68kUsage.values[m68kUsage.next] = (unsigned long)((double)busy * 1000.0 / budget + 0.5);
    m68kUsage.next = (m68kUsage.next + 1) % M68K_USAGE_HISTORY;
    if (m68kUsage.count < M68K_USAGE_HISTORY) ++m68kUsage.count;
}
unsigned M68K_Usage_GetHistory(unsigned long *values)
{
    unsigned i;
    for (i = 0; i < m68kUsage.count; ++i)
        values[i] = m68kUsage.values[(m68kUsage.next + M68K_USAGE_HISTORY - m68kUsage.count + i) % M68K_USAGE_HISTORY];
    return m68kUsage.count;
}

/* Read only instruction-fetch mappings: profiling must not read MMIO or
 * acknowledge a device. Starscream's instruction memory is word-swapped. */
static int Usage68K_Word(struct S68000CONTEXT *cpu, unsigned pc, unsigned *op)
{
    unsigned i;
    if ((pc & 1) || pc > 0xfffffe || !cpu->fetch) return 0;
    for (i = 0; i < 64; ++i) {
        struct STARSCREAM_PROGRAMREGION *f = &cpu->fetch[i];
        unsigned short raw;
        if (f->lowaddr == (unsigned)-1) break;
        if (pc < f->lowaddr || pc >= f->highaddr) continue;
        memcpy(&raw, (const void *)(f->offset + pc), sizeof(raw));
        *op = raw;
        return 1;
    }
    return 0;
}
/* Stable memory addresses only: no postincrement/predecrement or indexed
 * traversal. The only register writes accepted below are data loads/masks. */
static int Usage68K_MemoryEA(unsigned ea)
{
    unsigned mode = ea >> 3, reg = ea & 7;
    if (mode == 2) return 0;
    if (mode == 5) return 2;
    if (mode == 7 && (reg == 0 || reg == 2)) return 2;
    if (mode == 7 && reg == 1) return 4;
    return -1;
}
unsigned M68K_Polling_Loop(struct S68000CONTEXT *cpu)
{
    unsigned pc = cpu->pc, branch, op, start, at, word, offset;
    if (pc & 1) return 0;
    for (branch = pc; branch <= pc + 32 && branch <= 0xfffffc; branch += 2) {
        int disp, length, loaded = 0, tested = 0, valid = 1;
        if (!Usage68K_Word(cpu, branch, &op)) break;
        if ((op & 0xf000) != 0x6000) continue;
        disp = (signed char)op;
        if (!(op & 255)) {
            if (!Usage68K_Word(cpu, branch + 2, &word)) continue;
            disp = (short)word;
        }
        start = branch + 2 + disp;
        /* BRA to itself is an explicit spin wait. */
        if ((op & 0xff00) == 0x6000 && start == branch && pc == branch) return start + 1;
        /* Conditional backward branches only; never BSR or DBRA counters. */
        if ((op & 0xff00) < 0x6200 || disp >= -2 || disp < -32 ||
            start > pc || pc > branch || (start & 1)) continue;
        for (at = start; at < branch; at += length) {
            unsigned ea, size;
            int extra;
            if (!Usage68K_Word(cpu, at, &op)) { valid = 0; break; }
            ea = op & 63; size = (op >> 6) & 3;
            extra = Usage68K_MemoryEA(ea);
            length = 2;
            if ((op & 0xc1c0) == 0 && (op & 0x3000) && extra >= 0) {
                /* MOVE.B/W/L memory,Dn. MOVEA and stores are excluded. */
                length += extra; loaded = 1; tested = 1;
            } else if ((op & 0xff00) == 0x4a00 && size < 3 &&
                       ((ea >> 3) == 0 || extra >= 0)) {
                /* TST.B/W/L Dn or memory. */
                length += extra < 0 ? 0 : extra;
                tested = 1; if (extra >= 0) loaded = 1;
            } else if ((op & 0xffc0) == 0x0800 &&
                       ((ea >> 3) == 0 || extra >= 0)) {
                /* BTST #bit,Dn or memory (no bit writes). */
                length += 2 + (extra < 0 ? 0 : extra);
                tested = 1; if (extra >= 0) loaded = 1;
            } else if (((op & 0xff00) == 0x0200 || (op & 0xff00) == 0x0c00) &&
                       size < 3 && ((ea >> 3) == 0 || ((op & 0xff00) == 0x0c00 && extra >= 0))) {
                /* ANDI Dn or read-only CMPI Dn/memory. */
                length += (size == 2 ? 4 : 2) + (extra < 0 ? 0 : extra);
                tested = 1; if (extra >= 0) loaded = 1;
            } else if (op != 0x4e71) { valid = 0; break; }
            /* Ensure extension words exist, and the sampled PC is an opcode. */
            if (at < pc && at + length > pc) { valid = 0; break; }
            if (at + length > branch) { valid = 0; break; }
            for (offset = 2; offset < (unsigned)length; offset += 2)
                if (!Usage68K_Word(cpu, at + offset, &word)) valid = 0;
            if (!valid) break;
        }
        if (valid && at == branch && loaded && tested) return start + 1;
    }
    return 0;
}
static unsigned Usage68K_Exec(struct S68000CONTEXT *cpu, unsigned *idle,
    unsigned (*execute)(int), int cycles)
{
    unsigned loop, before, oldIdle, result, elapsed, sleeping;
    unsigned registers[16];
    unsigned short sr = cpu->sr;
    unsigned pending = cpu->interrupts[0];
    loop = M68K_Polling_Loop(cpu);
    before = cpu->odometer; oldIdle = *idle;
    if (loop) {
        memcpy(registers, cpu->dreg, sizeof(cpu->dreg));
        memcpy(registers + 8, cpu->areg, sizeof(cpu->areg));
    }
    result = execute(cycles);
    /* Statistics only: execute every instruction and retain IRQ timing.
     * Both ends must be in the same wait with stable registers/flags and no
     * pending interrupt, to avoid classifying computation or traversal idle. */
    if (loop && !pending && !cpu->interrupts[0] && sr == cpu->sr &&
        loop == M68K_Polling_Loop(cpu) &&
        !memcmp(registers, cpu->dreg, sizeof(cpu->dreg)) &&
        !memcmp(registers + 8, cpu->areg, sizeof(cpu->areg))) {
        elapsed = cpu->odometer - before;
        sleeping = *idle - oldIdle;
        if (elapsed > sleeping) *idle += elapsed - sleeping;
    }
    return result;
}
unsigned M68K_Exec_Profiled(int cycles)
{
    return Usage68K_Exec(&main68k_context, &main68k_idleCycles, main68k_exec, cycles);
}
unsigned S68K_Exec_Profiled(int cycles)
{
    return Usage68K_Exec(&sub68k_context, &sub68k_idleCycles, sub68k_exec, cycles);
}
