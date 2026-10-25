#include <stdio.h>
#include <stddef.h>
#include <windows.h>
#include "z80.h"
#include "Cpu_Z80.h"
#include "Mem_M68K.h"
#include "Mem_Z80.h"


/*** Z80_Reset - reset z80 sub-unit ***/

void Z80_Reset(void)
{
    Z80_Usage_ClearHistory();
	// Reset Z80

	memset(Ram_Z80, 0, 8 * 1024);

//	Bank_Z80 = 0x000000;
	Bank_Z80 = 0xFF8000;

	Z80_State &= 1;
//	Z80_State |= 2;		// RZWL needs it but Breath of Fire 3 don't want it...

	Last_BUS_REQ_Cnt = 0;
	Last_BUS_REQ_St = 0;

	z80_Reset(&M_Z80);
}

/*** Z80_Init - initialise this sub-unit ***/

int Z80_Init(void)
{
	// Init Z80

	z80_Init(&M_Z80);

	z80_Add_Fetch(&M_Z80, 0x00, 0x1F, &Ram_Z80[0]);
	z80_Add_Fetch(&M_Z80, 0x20, 0x3F, &Ram_Z80[0]);
	
	z80_Add_ReadB(&M_Z80, 0x00, 0xFF, Z80_ReadB_Bad);
	z80_Add_ReadB(&M_Z80, 0x00, 0x3F, Z80_ReadB_Ram);
	z80_Add_ReadB(&M_Z80, 0x40, 0x5F, Z80_ReadB_YM2612);
	z80_Add_ReadB(&M_Z80, 0x60, 0x6F, Z80_ReadB_Bank);
	z80_Add_ReadB(&M_Z80, 0x70, 0x7F, Z80_ReadB_PSG);
	z80_Add_ReadB(&M_Z80, 0x80, 0xFF, Z80_ReadB_68K_Ram);

	z80_Add_ReadW(&M_Z80, 0x00, 0xFF, Z80_ReadW_Bad);
	z80_Add_ReadW(&M_Z80, 0x00, 0x3F, Z80_ReadW_Ram);
	z80_Add_ReadW(&M_Z80, 0x40, 0x5F, Z80_ReadW_YM2612);
	z80_Add_ReadW(&M_Z80, 0x60, 0x6F, Z80_ReadW_Bank);
	z80_Add_ReadW(&M_Z80, 0x70, 0x7F, Z80_ReadW_PSG);
	z80_Add_ReadW(&M_Z80, 0x80, 0xFF, Z80_ReadW_68K_Ram);
	
	z80_Add_WriteB(&M_Z80, 0x00, 0xFF, Z80_WriteB_Bad);
	z80_Add_WriteB(&M_Z80, 0x00, 0x3F, Z80_WriteB_Ram);
	z80_Add_WriteB(&M_Z80, 0x40, 0x5F, Z80_WriteB_YM2612);
	z80_Add_WriteB(&M_Z80, 0x60, 0x6F, Z80_WriteB_Bank);
	z80_Add_WriteB(&M_Z80, 0x70, 0x7F, Z80_WriteB_PSG);
	z80_Add_WriteB(&M_Z80, 0x80, 0xFF, Z80_WriteB_68K_Ram);

	z80_Add_WriteW(&M_Z80, 0x00, 0xFF, Z80_WriteW_Bad);
	z80_Add_WriteW(&M_Z80, 0x00, 0x3F, Z80_WriteW_Ram);
	z80_Add_WriteW(&M_Z80, 0x40, 0x5F, Z80_WriteW_YM2612);
	z80_Add_WriteW(&M_Z80, 0x60, 0x6F, Z80_WriteW_Bank);
	z80_Add_WriteW(&M_Z80, 0x70, 0x7F, Z80_WriteW_PSG);
	z80_Add_WriteW(&M_Z80, 0x80, 0xFF, Z80_WriteW_68K_Ram);

	Z80_Reset();

	return 1;
}



#include "cd_sys.h"


void Write_To_Bank(int val)
{
#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "Z80 write bank : %d     Bank = %.8X\n", val, Bank_Z80);
#endif
}

void Read_To_68K_Space(int adr)
{
#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "Z80 read in 68K space : $(%.8X)\n", adr);
#endif
}

void Write_To_68K_Space(int adr, int data)
{
#ifdef DEBUG_CD
	fprintf(debug_SCD_file, "Z80 write in 68K space : $(%.8X) = %.8X\n", adr, data);
#endif
}

/* Telemetry lives outside the CPU context/save-state ABI. Core accounting
 * covers executions requested by both the scheduler and bus synchronization. */
static struct { unsigned long values[Z80_USAGE_HISTORY]; unsigned next, count; } z80Usage;
void Z80_Usage_ClearHistory(void) { memset(&z80Usage,0,sizeof(z80Usage)); Z80_UsageBusyCycles=0; }
void Z80_Usage_Begin_Frame(void) { Z80_UsageBusyCycles=0; }
void Z80_Usage_End_Frame(unsigned budget)
{
    unsigned busy=Z80_UsageBusyCycles;
    if (!budget) return;
    if (busy>budget) busy=budget;
    z80Usage.values[z80Usage.next]=(unsigned long)((double)busy*1000.0/budget+0.5);
    z80Usage.next=(z80Usage.next+1)%Z80_USAGE_HISTORY;
    if(z80Usage.count<Z80_USAGE_HISTORY) ++z80Usage.count;
}
unsigned Z80_Usage_GetHistory(unsigned long *values)
{
    unsigned i;
    for(i=0;i<z80Usage.count;++i)
        values[i]=z80Usage.values[(z80Usage.next+Z80_USAGE_HISTORY-z80Usage.count+i)%Z80_USAGE_HISTORY];
    return z80Usage.count;
}
/* Exact register countdowns with an optional balanced PUSH/POP timing pair.
 * Recognize only code with no calls, device writes or persistent memory work.
 * depth describes the one temporarily pushed word at the sampled PC. */
static unsigned Z80_Delay_Loop(Z80_CONTEXT *cpu, unsigned pc, unsigned *pair, unsigned *depth)
{
    unsigned branch;
    if(cpu!=&M_Z80 || pc>=0x4000) return 0;
    for(branch=pc;branch<=pc+16 && branch<0x3ffe;branch++) {
        unsigned start, at, reg, load, combine, op=Ram_Z80[branch&8191], pushed=0;
        if(op==0xc2) start=Ram_Z80[(branch+1)&8191]|(Ram_Z80[(branch+2)&8191]<<8);
        else if(op==0x20) start=branch+2+(signed char)Ram_Z80[(branch+1)&8191];
        else continue;
        if(start>pc || start>=branch || branch-start>8) continue;
        at=start; op=Ram_Z80[at&8191];
        if(op==0xc5 || op==0xd5 || op==0xe5 || op==0xf5) {
            if(Ram_Z80[(at+1)&8191]!=op-4) continue;
            pushed=1; at+=2;
        }
        op=Ram_Z80[at&8191];
        if(op==0x0b)reg=0;
        else if(op==0x1b)reg=1;
        else if(op==0x2b)reg=2;
        else continue;
        if(at+3!=branch)continue;
        load=Ram_Z80[(at+1)&8191]; combine=Ram_Z80[(at+2)&8191];
        /* LD A,low; OR A, or LD A,high; OR low. */
        if(!((load==0x79+reg*2 && combine==0xb7) ||
             (load==0x78+reg*2 && combine==0xb1+reg*2)))continue;
        *pair=reg; *depth=pushed && pc==start+1 ? 1 : 0;
        return start+1;
    }
    return 0;
}
/* Decode RAM opcodes only, never touch MMIO to inspect a polling address. */
unsigned Z80_Polling_Loop(Z80_CONTEXT *cpu)
{
    unsigned pc=z80_Get_PC(cpu), branch, start, at, op, length;
    unsigned delayPair, delayDepth, delay=Z80_Delay_Loop(cpu,pc,&delayPair,&delayDepth);
    if(delay)return delay;
    if(cpu!=&M_Z80 || pc>=0x4000) return 0;
    for(branch=pc;branch<=pc+32 && branch<0x3ffe;++branch) {
        int loaded=0, tested=0, valid=1;
        op=Ram_Z80[branch&0x1fff];
        /* A tight DJNZ/DEC loop is a deliberate timing delay, not a
         * command calculation. Only the exact register-only forms qualify. */
        if(op==0x10 && Ram_Z80[(branch+1)&0x1fff]==0xfe && pc==branch) return branch+1;
        if(op==0x20 && Ram_Z80[(branch+1)&0x1fff]==0xfd && branch>0 &&
            (Ram_Z80[(branch-1)&0x1fff]&0xc7)==0x05 &&
            Ram_Z80[(branch-1)&0x1fff]!=0x35 && pc>=branch-1) return branch;
        /* NOP padding followed by an unconditional backward jump is also
         * an explicit spin wait (e.g. EI; NOP; JP to the NOP). */
        if(op==0x18 || op==0xc3) {
            unsigned spinStart=op==0x18 ? branch+2+(signed char)Ram_Z80[(branch+1)&8191] :
                Ram_Z80[(branch+1)&8191]|(Ram_Z80[(branch+2)&8191]<<8);
            if(spinStart<=pc && spinStart<=branch && branch-spinStart<=32) {
                unsigned at;
                for(at=spinStart;at<branch;at++)if(Ram_Z80[at&8191]!=0)break;
                if(at==branch)return spinStart+1;
            }
        }
        if(op==0x20 || op==0x28 || op==0x30 || op==0x38)
            start=branch+2+(signed char)Ram_Z80[(branch+1)&0x1fff];
        else if(op==0xc2 || op==0xca || op==0xd2 || op==0xda)
            start=Ram_Z80[(branch+1)&0x1fff]|(Ram_Z80[(branch+2)&0x1fff]<<8);
        else continue;
        if(start>pc || start>=branch || branch-start>32) continue;
        for(at=start;at<branch;at+=length) {
            op=Ram_Z80[at&0x1fff]; length=1;
            if((op&0xc7)==0x46 && op!=0x76) loaded=1; /* LD r,(HL) */
            else if(op==0x0a || op==0x1a) loaded=1; /* LD A,(BC/DE) */
            else if(op==0x3a) { loaded=1; length=3; } /* LD A,(nn) */
            else if(op==0xe6 || op==0xf6 || op==0xfe) { tested=1; length=2; } /* AND/OR/CP imm */
            else if((op>=0xa0 && op<=0xa7) || (op>=0xb0 && op<=0xb7) || (op>=0xb8 && op<=0xbf)) {
                tested=1; if((op&7)==6)loaded=1; /* AND/OR/CP r or (HL) */
            } else if(op==0xcb && (Ram_Z80[(at+1)&0x1fff]&0xc0)==0x40) {
                tested=1; if((Ram_Z80[(at+1)&0x1fff]&7)==6)loaded=1;
                length=2; /* Read-only BIT n,r / (HL); never RES/SET/shift. */
            } else if(op==0xdd || op==0xfd) {
                unsigned next=Ram_Z80[(at+1)&0x1fff];
                if((next&0xc7)==0x46 && next!=0x76) { loaded=1; length=3; }
                else if(next==0xcb && (Ram_Z80[(at+3)&0x1fff]&0xc0)==0x40) {
                    loaded=tested=1; length=4; /* BIT n,(IX/IY+d) */
                } else { valid=0; break; }
            } else if(op!=0) { valid=0; break; }
            if(at+length>branch || (at<pc && at+length>pc)) { valid=0; break; }
        }
        if(valid && loaded && tested && at==branch) return start+1;
    }
    return 0;
}
/* Scanline-debt wait used by MD_FM. Unlike a simple polling loop this
 * updates B/C/D and has forward branches. Match the complete read-only
 * routine, including both exits, rather than treating arbitrary arithmetic
 * or backward branches as idle. Return only at instruction boundaries. */
static unsigned Z80_Scanline_Wait(Z80_CONTEXT *cpu)
{
    static const unsigned char body[]={
        0x3a,0x08,0x7f,0xb8,0x28,0x0d,0x57,0x90,0x42,
        0xfe,0xfa,0x38,0x02,0xd6,0xfa,0x57,0x79,0x92,
        0x4f,0x79,0xb7,0xfa
    };
    static const unsigned char offsets[]={0,3,4,6,7,8,9,11,13,15,16,17,18,19,20,21,22,23,24,27};
    unsigned pc=z80_Get_PC(cpu), i;
    if(cpu!=&M_Z80 || pc>=0x4000) return 0;
    for(i=0;i<sizeof(offsets);++i) {
        unsigned start, exit;
        const unsigned char *code;
        if(pc<offsets[i]) continue;
        start=pc-offsets[i];
        if((start&8191)+25>8192) continue;
        code=Ram_Z80+(start&8191);
        /* CD32X_Template calls this routine for each scanline. Its exits
         * are RET M / RET Z rather than jumps to the command dispatcher.
         * Match every byte and boundary; update_clock is useful accounting
         * work and must not be mistaken for this timing wait. */
        if(offsets[i]<=23 &&
            !memcmp(code,body,21) && code[21]==0xf8 && code[22]==0xc8 &&
            code[23]==0x18 && code[24]==0xe7)
            return start+1;
        if(offsets[i]==22 || offsets[i]==23 || (start&8191)+29>8192) continue;
        if(memcmp(code,body,sizeof(body)) || code[24]!=0xca ||
            code[27]!=0x18 || code[28]!=0xe3 ||
            code[22]!=code[25] || code[23]!=code[26]) continue;
        exit=code[22]|(code[23]<<8);
        if(exit>=start && exit<start+29) continue;
        return start+1;
    }
    return 0;
}
unsigned FASTCALL Z80_Exec_Profiled(Z80_CONTEXT *cpu, int target)
{
    if(cpu->CycleCnt>=(unsigned)target) return z80_Exec(cpu,target);
    /* Execute recognized clock waits normally, one instruction at a time.
     * Account only those instructions: an exit followed by sound writes in
     * the same scheduler slice must still count as work. Never skip MMIO.
     * Pending interrupts stay on the normal accounting path. A wait entered
     * later in an ordinary slice remains busy until the next call, so this
     * conservative estimate can still overstate the actual sound work. */
    while(cpu->CycleCnt<(unsigned)target && Z80_Scanline_Wait(cpu) &&
        !(cpu->IntLine & (0x80 | cpu->IFF.b.IFF1)) && !cpu->Status) {
        unsigned beforeWait=Z80_UsageBusyCycles, cycles=cpu->CycleCnt;
        unsigned waitResult=z80_Exec(cpu,cycles+1);
        Z80_UsageBusyCycles=beforeWait;
        if(waitResult || cpu->CycleCnt<=cycles) return waitResult;
    }
    if(cpu->CycleCnt>=(unsigned)target) return 0;
    unsigned loop=Z80_Polling_Loop(cpu), before=Z80_UsageBusyCycles, result;
    Z80_CONTEXT saved;
    if(loop) saved=*cpu;
    result=z80_Exec(cpu,target);
    if(loop) {
        unsigned pairBefore, pairAfter, depthBefore, depthAfter;
        unsigned delayBefore=Z80_Delay_Loop(cpu,z80_Get_PC(&saved),&pairBefore,&depthBefore);
        if(delayBefore==loop && Z80_Delay_Loop(cpu,z80_Get_PC(cpu),&pairAfter,&depthAfter)==loop && pairBefore==pairAfter) {
            if((unsigned short)(saved.SP.w.SP+depthBefore*2)!=(unsigned short)(cpu->SP.w.SP+depthAfter*2)) loop=0;
            else {
                saved.AF=cpu->AF;
                if(pairBefore==0)saved.BC=cpu->BC;
                else if(pairBefore==1)saved.DE=cpu->DE;
                else saved.HL=cpu->HL;
                saved.SP=cpu->SP;
            }
        }
    }
    if(loop && loop==Z80_Polling_Loop(cpu)) {
        unsigned at=loop-1, op=Ram_Z80[at&0x1fff];
        if(op==0x10 && Ram_Z80[(at+1)&0x1fff]==0xfe) saved.BC.b.B=cpu->BC.b.B;
        else if((op&0xc7)==0x05 && op!=0x35 && Ram_Z80[(at+1)&0x1fff]==0x20 &&
            Ram_Z80[(at+2)&0x1fff]==0xfd) {
            switch((op>>3)&7) {
            case 0: saved.BC.b.B=cpu->BC.b.B; break;
            case 1: saved.BC.b.C=cpu->BC.b.C; break;
            case 2: saved.DE.b.D=cpu->DE.b.D; break;
            case 3: saved.DE.b.E=cpu->DE.b.E; break;
            case 4: saved.HL.b.H=cpu->HL.b.H; break;
            case 5: saved.HL.b.L=cpu->HL.b.L; break;
            case 7: saved.AF.b.A=cpu->AF.b.A; break;
            }
            saved.AF.b.F=cpu->AF.b.F;
            saved.AF.b.FXY=cpu->AF.b.FXY;
        }
    }
    /* R changes on instruction fetch. PC may be anywhere in the same loop. */
    if(loop && !(saved.IntLine & (0x80 | saved.IFF.b.IFF1)) &&
        !(cpu->IntLine & (0x80 | cpu->IFF.b.IFF1)) && loop==Z80_Polling_Loop(cpu) &&
        !memcmp(&saved.AF,&cpu->AF,offsetof(Z80_CONTEXT,PC)) &&
        !memcmp(&saved.SP,&cpu->SP,offsetof(Z80_CONTEXT,R)-offsetof(Z80_CONTEXT,SP)) &&
        saved.Status==cpu->Status)
        Z80_UsageBusyCycles=before;
    return result;
}
