#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/Gens/SH2.h"
static SH2_CONTEXT cpu;
static unsigned short code[16];
static void setup(unsigned short op) {
    memset(&cpu, 0, sizeof(cpu));
    SH2_Init(&cpu, 0);
    cpu.Status = 0;
    code[0]=(op>>8)|(op<<8); code[1]=0x0900; code[2]=0xfeaf; code[3]=0x0900;
    SH2_Set_Fetch_Reg(&cpu,0,0x06000000,0x0600001f,code);
    SH2_Set_PC(&cpu,0x06000004);
}
static UINT32 FASTCALL poll_read(UINT32 address) { return 0; }
static void put(unsigned i, unsigned op) { code[i]=(op>>8)|(op<<8); }
static void polling_tests() {
    SH2_CONTEXT raw;
    setup(0x6012); // MOV.L @R1,R0; TST R0,R0; BT back to load
    put(1,0x2008); put(2,0x89fc); put(3,0x0009);
    cpu.R[1]=0x07000000;
    SH2_Add_ReadL(&cpu,7,7,poll_read);
    assert(SH2_Polling_Loop(&cpu)==0x06000001);
    raw=cpu;
    for(unsigned i=1;i<=100;++i) {
        SH2_Exec_Profiled(&cpu,i*100);
        SH2_Exec(&raw,i*100);
    }
    assert(cpu.PC==raw.PC && cpu.Odometer==raw.Odometer);
    assert(!memcmp(cpu.R,raw.R,sizeof(cpu.R)));
    assert(!memcmp(&cpu.SR,&raw.SR,sizeof(cpu.SR)));
    SH2_Usage_End_Frame(&cpu); assert(cpu.Usage_PerMille<50);
    put(1,0x7001); // ADD #1,R0: computation must not count as idle
    assert(!SH2_Polling_Loop(&cpu));
    setup(0x6016); // postincrementing load: memory traversal, not polling
    put(1,0x2008); put(2,0x89fc);
    assert(!SH2_Polling_Loop(&cpu));
    setup(0x6012); put(1,0x2102); put(2,0x89fc); // store
    assert(!SH2_Polling_Loop(&cpu));
    setup(0x6112); put(1,0x2118); put(2,0x89fc); // changes address register
    assert(!SH2_Polling_Loop(&cpu));
    setup(0xaffe);
    assert(SH2_Polling_Loop(&cpu)==0x06000001);
    SH2_Exec_Profiled(&cpu,1000); SH2_Usage_End_Frame(&cpu);
    assert(cpu.Usage_PerMille<50);
    puts("PASS: polling/spin wait estimation, computation/store/traversal exclusion, unchanged execution");
}
static void history_tests() {
    UINT32 values[SH2_USAGE_HISTORY];
    SH2_Usage_ClearHistory(&M_SH2); SH2_Usage_ClearHistory(&S_SH2);
    for(unsigned i=0;i<300;++i) {
        M_SH2.Odometer=1000; M_SH2.Idle_Cycles=1000-i;
        SH2_Usage_End_Frame(&M_SH2);
    }
    assert(SH2_Usage_GetHistory(&M_SH2,values)==256);
    assert(values[0]==44 && values[255]==299);
    assert(SH2_Usage_GetHistory(&S_SH2,values)==0);
    assert(SH2_Usage_GetHistory(&M_SH2,values)==256); // reading doesn't advance
    SH2_Usage_ClearHistory(&M_SH2);
    assert(!SH2_Usage_GetHistory(&M_SH2,values));
    puts("PASS: 256-frame chronological history, rollover, independent CPUs, clear");
}
int main() {
    setup(0x7001); put(1,0xaffd); put(2,9); // arithmetic loop
    SH2_Exec(&cpu,1000); SH2_Usage_End_Frame(&cpu);
    assert(cpu.Usage_PerMille==1001);
    setup(0x001b); // SLEEP: count the instruction but not remaining slice
    SH2_Exec(&cpu,1000); SH2_Usage_End_Frame(&cpu);
    assert(cpu.Usage_PerMille>=1 && cpu.Usage_PerMille<=3);
    cpu.Idle_Cycles=0; SH2_Clear_Odo(&cpu);
    SH2_Exec(&cpu,1000); SH2_Usage_End_Frame(&cpu);
    assert(cpu.Usage_PerMille==1);
    setup(0x7001); put(1,0xaffd); put(2,9); SH2_Disable(&cpu);
    SH2_Exec(&cpu,1000); SH2_Usage_End_Frame(&cpu);
    assert(cpu.Usage_PerMille==1);
    SH2_Enable(&cpu); cpu.Idle_Cycles=0; SH2_Clear_Odo(&cpu);
    SH2_Exec(&cpu,1000); SH2_Usage_End_Frame(&cpu);
    assert(cpu.Usage_PerMille==1001);
    cpu.Odometer=1000; cpu.Idle_Cycles=500;
    SH2_Usage_End_Frame(&cpu); assert(cpu.Usage_PerMille==501);
    SH2_Usage_End_Frame(&cpu); assert(cpu.Usage_PerMille==501);
    cpu.Odometer=0; SH2_Usage_End_Frame(&cpu); assert(!cpu.Usage_PerMille);
    polling_tests();
    history_tests();
    puts("PASS: busy, sleep entry, halted, disabled, resumed, 50%, empty sample");
}
