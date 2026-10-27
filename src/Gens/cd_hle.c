/* BIOS call emulation for disc boot, CD reads/audio and game saves.
 * No original firmware bytes are included. This is a partial BIOS API.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "G_gfx.h"
#include <mmsystem.h>
#include "G_dsound.h"
#include "cd_hle.h"
#include "cd_transfer.h"
#include "word_ram_transfer.h"
#include "main_word_ram_transfer.h"
#include "genesis_ram_transfer.h"
#include "vram_transfer.h"
#include "program_ram_transfer.h"
#include "Cpu_68k.h"
#include "Mem_M68K.h"
#include "Mem_S68K.h"
#include "Mem_Z80.h"
#include "cd_file.h"
#include "cd_sys.h"
#include "LC89510.h"
#include "Rom.h"
#include "vdp_io.h"
#include "gens.h"
#include "G_main.h"
#include "save.h"

int CD_HLE_Active;
static unsigned char boot[0x10000];
static unsigned ip_off, ip_size, sp_off, sp_size;
static unsigned read_lba, read_count;
static int read_paused;
static unsigned read_budget, read_clock;
static unsigned sectors_read;
static int transfer_counted; /* One payload per pending sector, even when polled/re-read. */
static void count_data_sector(void) {
    if (!transfer_counted) CDTransfer_AddBytes(2048);
    transfer_counted = 1;
}
static unsigned frame;
static int native_irq2;
static int comm_wait;
static int audio_track=-1, audio_sector, audio_mode, audio_paused, audio_clock;
static unsigned audio_seek_delay;
static unsigned volume=1024, master_volume=1024, fade_target=1024, fade_step;
static unsigned main_seen[256], sub_seen[256];
static FILE *trace;
typedef struct { unsigned char name[11],mode; unsigned blocks; unsigned char data[8000]; } HLE_Save;
static HLE_Save saves[16], previous_saves[16];
static char save_path[1200];
static unsigned disc_id;
static int saves_bad;


static unsigned be32(const unsigned char *p) { return ((unsigned)p[0]<<24)|((unsigned)p[1]<<16)|((unsigned)p[2]<<8)|p[3]; }
static unsigned rw(const unsigned char *p, unsigned a) { return p[a^1]*256+p[(a+1)^1]; }
static unsigned rl(const unsigned char *p, unsigned a) { return (rw(p,a)<<16)|rw(p,a+2); }
static void ww(unsigned char *p,unsigned a,unsigned v) { p[a^1]=(unsigned char)(v>>8); p[(a+1)^1]=(unsigned char)v; }
static void wl(unsigned char *p,unsigned a,unsigned v) { ww(p,a,v>>16); ww(p,a+2,v); }
static void copy_be(unsigned char *p,unsigned a,const unsigned char *b,unsigned n) { unsigned i; for(i=0;i<n;i++)p[(a+i)^1]=b[i]; }
/* HLE CPU data operations keep their existing direct-RAM semantics.
 * Boot patching uses the unmeasured helpers above; CDC DMA has its own path. */
static unsigned prg_rb(unsigned a) { ProgramRAMTransfer_AddBytes(0,1); return Ram_Prg[a^1]; }
static unsigned prg_rw(unsigned a) { ProgramRAMTransfer_AddBytes(0,2); return rw(Ram_Prg,a); }
static void prg_wb(unsigned a,unsigned v) { ProgramRAMTransfer_AddBytes(1,1); Ram_Prg[a^1]=(unsigned char)v; }
static void prg_ww(unsigned a,unsigned v) { ProgramRAMTransfer_AddBytes(1,2); ww(Ram_Prg,a,v); }
static void prg_wl(unsigned a,unsigned v) { prg_ww(a,v>>16); prg_ww(a+2,v); }
static void prg_copy(unsigned a,const unsigned char *b,unsigned n) { ProgramRAMTransfer_AddBytes(1,n); copy_be(Ram_Prg,a,b,n); }
static void prg_clear(unsigned a,unsigned n) { ProgramRAMTransfer_AddBytes(1,n); memset(Ram_Prg+a,0,n); }
static unsigned main_rb(unsigned a) { GenesisRAMTransfer_AddMainBytes(0,1); return Ram_68k[a^1]; }
static unsigned main_rw(unsigned a) { GenesisRAMTransfer_AddMainBytes(0,2); return rw(Ram_68k,a); }
static void main_wb(unsigned a,unsigned v) { GenesisRAMTransfer_AddMainBytes(1,1); Ram_68k[a^1]=(unsigned char)v; }
static void main_ww(unsigned a,unsigned v) { GenesisRAMTransfer_AddMainBytes(1,2); ww(Ram_68k,a,v); }
static void main_wl(unsigned a,unsigned v) { main_ww(a,v>>16); main_ww(a+2,v); }
static void main_clear(unsigned a,unsigned n) { GenesisRAMTransfer_AddMainBytes(1,n); memset(Ram_68k+a,0,n); }
static unsigned sector_header(unsigned lba) {
    unsigned t=lba+150,m=t/4500,s=t/75%60,f=t%75;
    return ((m/10*16+m%10)<<24)|((s/10*16+s%10)<<16)|((f/10*16+f%10)<<8)|1;
}
static unsigned bcd(unsigned v) { return (v/10)*16+v%10; }
static int sector(unsigned lba, unsigned char *b, unsigned bytes) {
    unsigned stride=Tracks[0].Type==TYPE_BIN?2352:2048;
    if(!Tracks[0].F || Tracks[0].Lenght<=0 || lba >= (unsigned)Tracks[0].Lenght || lba>0x7fffffffU/stride)return 0;
    if(fseek(Tracks[0].F,lba*stride+(stride==2352?16:0),SEEK_SET))return 0;
    if(stride==2048 && bytes>2048) { memset(b+2048,0,bytes-2048); bytes=2048; }
    return fread(b,1,bytes,Tracks[0].F)==bytes;
}
static unsigned memr(int sub,unsigned a) {
    a &= 0xffffff;
    if(sub) { if(a<0x80000)return prg_rw(a); return S68K_Data_RW(a); }
    if(a>=0xff0000)return main_rw(a&0xffff);
    if(a<0x20000)return rw(Rom_Data,a);
    return M68K_Data_RW(a);
}
static void memw(int sub,unsigned a,unsigned v) {
    a &= 0xffffff;
    if(sub) { if(a<0x80000)prg_ww(a,v); else S68K_Data_WW(a,(unsigned short)v); }
    else { if(a>=0xff0000)main_ww(a&0xffff,v); else M68K_Data_WW(a,(unsigned short)v); }
}
static unsigned meml(int sub,unsigned a) { return (memr(sub,a)<<16)|memr(sub,a+2); }
static void ret(int sub) {
    struct S68000CONTEXT *c=sub?&sub68k_context:&main68k_context;
    c->pc=meml(sub,c->areg[7])&0xffffff; c->areg[7]+=4;
    c->interrupts[0]&=~(sub?1:0x10);
}
static void stub(unsigned char *p,unsigned a,unsigned gate) {
    /* BRA.W to an emulator-owned trampoline. Preserve SR across the host call. */
    ww(p,a,0x6000); ww(p,a+2,gate-a-2);
    ww(p,gate,0x40e7); /* MOVE SR,-(SP) */
    ww(p,gate+2,0x4e72); ww(p,gate+4,0x2700);
}
static void restore_sr(int sub) {
    struct S68000CONTEXT *c=sub?&sub68k_context:&main68k_context;
    c->sr=(unsigned short)memr(sub,c->areg[7]); c->areg[7]+=2;
}
static void unsupported(int sub,unsigned fn) {
    if(trace) { fprintf(trace,"UNSUPPORTED %s BIOS call %04x at frame %u\n",sub?"sub":"main",fn,frame); fflush(trace); }
    { char message[160]; sprintf(message,"BIOS HLE: unsupported %s call %04X. Game paused.",sub?"sub":"main",fn); Put_Info(message,10000); }
    Paused=1;
}

void CD_HLE_Close(void) {
    CD_HLE_Active=0;
    if(trace) { fclose(trace); trace=NULL; }
}

static void load_saves(void) {
    FILE *f; unsigned i, used=0;
    unsigned char header[16];
    const char *dir=getenv("GENS_HLE_SAVE_DIR");
    if(!dir)dir=BRAM_Dir[0]?BRAM_Dir:Gens_Path;
    memset(saves,0,sizeof(saves)); saves_bad=0;
    if(strlen(dir)>1023) { saves_bad=1; return; }
    sprintf(save_path,"%s%ssegacd-%08x.hbr",dir,dir[0] && dir[strlen(dir)-1]!='/' && dir[strlen(dir)-1]!='\\'?"/":"",disc_id);
    f=fopen(save_path,"rb"); if(!f) { if(errno!=ENOENT)saves_bad=1; return; }
    if(fread(header,1,8,f)!=8||memcmp(header,"GensHLE1",8)) { saves_bad=1; fclose(f); return; }
    for(i=0;i<16;i++) {
        unsigned size;
        if(fread(header,1,16,f)!=16) { saves_bad=1; break; }
        memcpy(saves[i].name,header,11); saves[i].mode=header[11];
        saves[i].blocks=header[12]*256+header[13];
        size=saves[i].blocks*(saves[i].mode?32:64);
        used+=saves[i].blocks;
        if(used>125 || (saves[i].mode!=0 && saves[i].mode!=255) || saves[i].blocks>125 || size>sizeof(saves[i].data) || fread(saves[i].data,1,size,f)!=size) { saves_bad=1; break; }
    }
    if(!saves_bad && fgetc(f)!=EOF)saves_bad=1;
    fclose(f);
}
static int write_saves(void) {
    char temp[1240]; FILE *f; unsigned i; int ok=1;
    sprintf(temp,"%s.tmp",save_path); f=fopen(temp,"wb"); if(!f)return 0;
    if(fwrite("GensHLE1",1,8,f)!=8)ok=0;
    for(i=0;i<16 && ok;i++) {
        unsigned char h[16]={0}; unsigned size=saves[i].blocks*(saves[i].mode?32:64);
        memcpy(h,saves[i].name,11); h[11]=saves[i].mode; h[12]=(unsigned char)(saves[i].blocks>>8); h[13]=(unsigned char)saves[i].blocks;
        if(fwrite(h,1,16,f)!=16 || fwrite(saves[i].data,1,size,f)!=size)ok=0;
    }
    if(fclose(f))ok=0;
    if(ok)ok=MoveFileExA(temp,save_path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok)remove(temp);
    return ok;
}
static void bramcall(void) {
    struct S68000CONTEXT *c=&sub68k_context;
    unsigned fn=c->dreg[0]&0xffff,a=c->areg[0],dest=c->areg[1],i,j,used=0,count=0;
    int found=-1,empty=-1; unsigned char name[11];
    for(i=0;i<11;i++)name[i]=(unsigned char)(memr(1,(a+i)&~1U)>>((a+i)&1?0:8));
    for(i=0;i<16;i++) {
        if(saves[i].blocks) { used+=saves[i].blocks; count++; if(!memcmp(name,saves[i].name,11))found=i; }
        else if(empty<0)empty=i;
    }
    if(trace)fprintf(trace,"BRAM fn=%u a0=%x a1=%x found=%d\n",fn,a,dest,found);
    c->sr &= ~1;
    if(saves_bad && fn!=6) { c->sr|=1; c->dreg[1]=2; ret(1); return; }
    if(fn==4 || fn==5 || fn==6)memcpy(previous_saves,saves,sizeof(saves));
    switch(fn) {
    case 0: c->dreg[0]=0x2000; c->dreg[1]=0; break;
    case 1: c->dreg[0]=used<=125?125-used:0; c->dreg[1]=count; break;
    case 2: case 3: case 8:
        if(found<0) { c->sr|=1; c->dreg[0]=0; break; }
        c->dreg[0]=saves[found].blocks; c->dreg[1]=saves[found].mode?0xffff:0;
        if(fn==3) {
            unsigned n=saves[found].blocks*(saves[found].mode?32:64);
            if(dest>=0x80000 || n>0x80000-dest) { c->sr|=1; break; }
            prg_copy(dest,saves[found].data,n);
        } else if(fn==8) {
            unsigned n=saves[found].blocks*(saves[found].mode?32:64);
            if(dest>=0x80000 || n>0x80000-dest) { c->sr|=1; break; }
            c->dreg[0]=0;
            for(j=0;j<n;j++)if(prg_rb(dest+j)!=saves[found].data[j]) { c->sr|=1; c->dreg[0]=0xffffffff; break; }
        }
        break;
    case 4: {
        unsigned blocks=memr(1,a+12),mode=memr(1,a+10)&255,n=blocks*(mode?32:64);
        int slot=found>=0?found:empty;
        if(slot<0||!blocks||blocks>125||n>8000||dest>=0x80000||n>0x80000-dest||used-(found>=0?saves[found].blocks:0)+blocks>125) { c->sr|=1; break; }
        memcpy(saves[slot].name,name,11); saves[slot].mode=(unsigned char)mode; saves[slot].blocks=blocks;
        for(j=0;j<n;j++)saves[slot].data[j]=(unsigned char)prg_rb(dest+j);
        if(!write_saves())c->sr|=1;
        break;
    }
    case 5: if(found<0)c->sr|=1; else { saves[found].blocks=0; if(!write_saves())c->sr|=1; } break;
    case 6: memset(saves,0,sizeof(saves)); if(!write_saves())c->sr|=1; else saves_bad=0; break;
    case 7: { /* BRMDIR: filtered directory, with skip/count in D1. */
        unsigned skip=c->dreg[1]>>16, capacity=c->dreg[1]&0xffff, written=0, offset=0;
        if(dest>=0x80000 || capacity>(0x80000-dest)/16) { c->sr|=1; break; }
        for(i=0;i<16;i++) {
            if(!saves[i].blocks)continue;
            unsigned start=offset;
            offset+=saves[i].blocks*64;
            for(j=0;j<11 && name[j]!='*' && name[j]==saves[i].name[j];j++);
            if(j<11 && name[j]!='*')continue;
            if(skip) { skip--; continue; }
            if(written==capacity) { c->sr|=1; break; }
            prg_copy(dest+written*16,saves[i].name,11);
            prg_wb(dest+written*16+11,0);
            prg_ww(dest+written*16+12,start);
            prg_ww(dest+written*16+14,saves[i].blocks);
            written++;
        }
        for(j=written*16;j<capacity*16;j++)prg_wb(dest+j,0);
        break;
    }
    default: unsupported(1,0xb000+fn); c->sr|=1; break;
    }
    if((fn==4 || fn==5 || fn==6) && (c->sr&1))memcpy(saves,previous_saves,sizeof(saves));
    ret(1);
}

int CD_HLE_Prepare(void) {
    unsigned i;
    CD_HLE_Active=0;
    for(i=0;i<sizeof(boot)/2048;i++)if(!sector(i,boot+i*2048,2048))return 0;
    if(memcmp(boot,"SEGADISCSYSTEM",14))return 0;
    ip_off=be32(boot+0x30); ip_size=be32(boot+0x34);
    /* First-sector IP (the header may give the next sector, $800).
     * Its security/startup block begins immediately after the disc header. */
    if(ip_off==0x800) {
        /* The IP length describes data following the first-sector security
         * block; both that $600-byte block and the IP are handed to main RAM. */
        if(ip_size>0xf000-0x600)return 0;
        ip_off=0x200; ip_size+=0x600;
    }
    sp_off=be32(boot+0x40); sp_size=be32(boot+0x44);
    if((ip_off|ip_size|sp_off|sp_size)&1)return 0;
    if(ip_size<2)return 0;
    if(ip_off>=sizeof(boot)||ip_size>0xf000||ip_size>sizeof(boot)-ip_off ||
       sp_off>=sizeof(boot)||sp_size>sizeof(boot)-sp_off||sp_size<0x28)return 0;
    Free_Rom(Game);
    Game=(struct Rom*)calloc(1,sizeof(struct Rom));
    if(!Game)return 0;
    Rom_Size=0x20000;
    memset(Rom_Data,0,Rom_Size);
    /* The caller swaps this synthetic ROM before resetting the CPUs. */
    Rom_Data[1]=0xff; Rom_Data[2]=0xfd; Rom_Data[5]=0xff;
    disc_id=2166136261U;
    for(i=0;i<sizeof(boot);i++)disc_id=(disc_id^boot[i])*16777619U;
    load_saves();
    CD_HLE_Active=1;
    return 1;
}

int CD_HLE_Boot(void) {
    unsigned i,table,init,main,irq;
    /* The disc may release Z80 reset before installing a sound driver.
     * Empty RAM would run through the banked 68K window and corrupt RAM.
     * Supply our own DI / JP $0001 idle loop, replaceable by the game. */
    Ram_Z80[0]=0xf3; Ram_Z80[1]=0xc3; Ram_Z80[2]=1; Ram_Z80[3]=0;
    transfer_counted=0;
    CDTransfer_Reset();
    read_lba=read_count=frame=0; read_budget=1; read_clock=0; read_paused=0; sectors_read=0;
    audio_track=-1; audio_sector=audio_clock=audio_paused=0;
    audio_seek_delay=0; native_irq2=0; comm_wait=0;
    volume=master_volume=fade_target=1024; fade_step=0;
    memset(main_seen,0,sizeof(main_seen)); memset(sub_seen,0,sizeof(sub_seen));
    if(trace)fclose(trace);
    trace=getenv("GENS_HLE_TRACE")?fopen(getenv("GENS_HLE_TRACE"),"w"):NULL;
    copy_be(Ram_68k,0,boot+ip_off,ip_size);
    copy_be(Ram_Prg,0x6000,boot+sp_off,sp_size);
    table=0x6000+be32(boot+sp_off+0x18);
    if(table<0x6000||table+8>=0x6000+sp_size)return 0;
    init=table+rw(Ram_Prg,table); main=table+rw(Ram_Prg,table+2); irq=table+rw(Ram_Prg,table+4);
    if((init|main|irq)&1 || init>=0x6000+sp_size || main>=0x6000+sp_size || irq>=0x6000+sp_size)return 0;
    for(i=0x200;i<0x400;i+=4)stub(Rom_Data,i,0x2000+(i-0x200)*2);
    /* Interrupt vectors point at writable, BIOS-compatible RAM trampolines. */
    for(i=8;i<0x100;i+=4)wl(Rom_Data,i,0x1800);
    ww(Rom_Data,0x1800,0x4238); ww(Rom_Data,0x1802,0xfe26); ww(Rom_Data,0x1804,0x08f9); ww(Rom_Data,0x1806,0);
    wl(Rom_Data,0x1808,0xa12000); ww(Rom_Data,0x180c,0x4e73);
    /* Wait for the game's V-blank handler to acknowledge the BIOS work flag. */
    ww(Rom_Data,0x1840,0x4a38); ww(Rom_Data,0x1842,0xfe26);
    ww(Rom_Data,0x1844,0x66fa); ww(Rom_Data,0x1846,0x4e75);
    wl(Rom_Data,0x78,0xfffd06); ww(Ram_68k,0xfd06,0x4ef9); wl(Ram_68k,0xfd08,0x1800);
    ww(Rom_Data,0x1860,0x4e75);
    ww(Ram_68k,0xfda8,0x4ef9); wl(Ram_68k,0xfdaa,0x1860); /* Writable V-blank callback. */
    /* _MBURAM is the main CPU's optional backup RAM cartridge entry.
     * No cartridge is attached: BRMINIT returns carry set and D1.w=0.
     * Keep this separate from the sub CPU's internal backup RAM API.
     * SEGA-CD Technical Bulletins, 6/16/92, BIOS specifications correction. */
    ww(Ram_68k,0xfdae,0x4ef9); wl(Ram_68k,0xfdb0,0x1868);
    ww(Rom_Data,0x1868,0x7200); /* MOVEQ #0,D1 */
    ww(Rom_Data,0x186a,0x003c); ww(Rom_Data,0x186c,1); /* ORI #1,CCR */
    ww(Rom_Data,0x186e,0x4e75);
    wl(Rom_Data,0x70,0xfffd0c); ww(Ram_68k,0xfd0c,0x4ef9); wl(Ram_68k,0xfd0e,0x1800);
    for(i=0;i<16;i++) {
        wl(Rom_Data,0x80+i*4,0xfffd18+i*6);
        ww(Ram_68k,0xfd18+i*6,0x4e73);
    }
    /* BIOS enables interrupts again after USER_INIT, even if the game
     * temporarily masks IRQ2 while initializing its own state. */
    ww(Ram_Prg,0x1000,0x4eb9); wl(Ram_Prg,0x1002,init);
    ww(Ram_Prg,0x1006,0x027c); ww(Ram_Prg,0x1008,0xf8ff);
    ww(Ram_Prg,0x100a,0x4ef9); wl(Ram_Prg,0x100c,main);
    for(i=8;i<0x100;i+=4)wl(Ram_Prg,i,0x1100);
    ww(Ram_Prg,0x1100,0x4e73);
    /* Graphics/timer users patch only the destination, leaving the BIOS JMP. */
    ww(Ram_Prg,0x5f76,0x4ef9); wl(Ram_Prg,0x5f78,0x1100);
    wl(Ram_Prg,0x64,0x5f76);
    ww(Ram_Prg,0x5f82,0x4ef9); wl(Ram_Prg,0x5f84,0x1100);
    wl(Ram_Prg,0x6c,0x5f82);
    /* TRAP vectors dispatch through the BIOS-compatible writable jump table. */
    for(i=0;i<16;i++) {
        wl(Ram_Prg,0x80+i*4,0x5fa0+i*6);
        ww(Ram_Prg,0x5fa0+i*6,0x4e73);
    }
    wl(Ram_Prg,0x68,0x1120);
    ww(Ram_Prg,0x1120,0x48e7); ww(Ram_Prg,0x1122,0xfffe);
    /* USER call 2 is writable; the SP header supplies its initial target. */
    ww(Ram_Prg,0x5f34,0x4ef9); wl(Ram_Prg,0x5f36,irq);
    ww(Ram_Prg,0x1124,0x4eb9); wl(Ram_Prg,0x1126,0x5f34);
    ww(Ram_Prg,0x112a,0x4cdf); ww(Ram_Prg,0x112c,0x7fff); ww(Ram_Prg,0x112e,0x4e73);
    /* User-call entry (fourth SP jump-table entry). */
    if(rw(Ram_Prg,table+6)) {
        unsigned user=table+(short)rw(Ram_Prg,table+6);
        if((user&1)||user<0x6000||user>=0x6000+sp_size)return 0;
        ww(Ram_Prg,0x5f3a,0x4ef9); wl(Ram_Prg,0x5f3c,user);
    } else ww(Ram_Prg,0x5f3a,0x4e75);
    stub(Ram_Prg,0x5f22,0x1200); stub(Ram_Prg,0x5f16,0x1210);
    main68k_context.pc=0xff0000; main68k_context.areg[7]=0xfffd00; main68k_context.sr=0x2000;
    sub68k_context.pc=0x1000; sub68k_context.areg[7]=0x5e00; sub68k_context.sr=0x2000;
    S68K_State=1; Int_Mask_S68K=4; Ram_Word_State=0; MS68K_Set_Word_Ram();
    /* CD boot hands the program 1M banks; games can then select 2M mode. */
    S68K_WB(0xff8003,4);
    /* BIOS TOC first/last track cache, also read directly by disc programs. */
    ww(Ram_Prg,0x5e90,0x100|SCD.TOC.Last_Track);
    /* Games also access the BIOS VDP register shadow directly. */
    { static const unsigned char defaults[19]={4,0x24,0x30,0x28,7,0x5c,0,0,0,0,0,0,0x81,0x2f,0,2,0x11,0,0};
      for(i=0;i<19;i++) { Set_VDP_Reg(i,defaults[i]); ww(Ram_68k,0xfdb4+i*2,0x8000+i*0x100+defaults[i]); }
    }
    if(trace)fprintf(trace,"boot ip=%x/%x sp=%x/%x init=%x main=%x irq=%x\n",ip_off,ip_size,sp_off,sp_size,init,main,irq);
    return 1;
}

int CD_HLE_AudioPlaying(void) { return CD_HLE_Active && audio_track>=1 && !audio_paused; }

static void audio_tick(void) {
    short samples[1176];
    int clock_rate=Sound_Rate>0?Sound_Rate:44100;
    int frame_samples=Seg_Lenght>0?Seg_Lenght:clock_rate/(CPU_Mode?50:60);
    /* Seeking finishes in pause. MSCSEEK1 selects one-shot playback when
     * MSCPAUSEOFF arrives; it must not start playback on its own. */
    if(audio_seek_delay)audio_seek_delay--;
    if(audio_track<1 || audio_paused)return;
    /* Match the samples actually consumed by the mixer. At 22050 Hz NTSC,
     * each frame consumes 368 samples, not the nominal 367.5. Scheduling
     * exactly 75 sectors per 60 frames slowly empties the CD audio buffer. */
    audio_clock+=frame_samples*75;
    while(audio_clock>=clock_rate && audio_track>=1) {
        struct _file_track *t=&Tracks[audio_track];
        audio_clock-=clock_rate;
        if(volume<fade_target)volume+=fade_target-volume<fade_step?fade_target-volume:fade_step;
        else if(volume>fade_target)volume-=volume-fade_target<fade_step?volume-fade_target:fade_step;
        if(audio_sector>=t->Lenght) {
            if(audio_mode==0x13)audio_sector=0;
            else if(audio_mode==0x11 && audio_track+1<SCD.TOC.Last_Track) { audio_track++; audio_sector=0; t=&Tracks[audio_track]; }
            else { audio_paused=1; break; }
        }
        if(t->Type!=TYPE_CDDA || !t->F || fseek(t->F,(t->FileOffset+audio_sector)*2352,SEEK_SET) || fread(samples,1,2352,t->F)!=2352) {
            unsupported(1,0x12); audio_track=-1; break;
        }
        { unsigned j, gain=volume<master_volume?volume:master_volume;
          for(j=0;j<1176;j++)samples[j]=(short)(samples[j]*(int)gain/1024); }
        Write_CD_Audio(samples,44100,2,588);
        audio_sector++;
    }
}

void CD_HLE_Frame(void) {
    if(!CD_HLE_Active)return;
    frame++;
    /* CDCSTAT must expose the disc's 75-sector/s cadence. Streaming games
     * otherwise overrun their compressed video/PCM rings in program RAM.
     * Apply this to BIOS transfers too, even if no host port was accessed. */
    read_clock+=75;
    read_budget+=read_clock/(CPU_Mode?50:60);
    read_clock%=CPU_Mode?50:60;
    if(read_budget>2)read_budget=2;
    audio_tick();
    /* Older IPs rely on the BIOS heartbeat; IPs that assert IRQ2 themselves
     * own its timing (notably during sound-driver handshakes). This includes
     * custom SET_VINT handlers that call the BIOS IRQ2 entry. */
    if(!native_irq2)sub68k_interrupt(2,-1);
    if(trace && frame%60==0) { fprintf(trace,"frame %u main=%06x sub=%06x comm=%04x/%04x word=%u sr=%x/%x sectors=%u\n",frame,main68k_context.pc,sub68k_context.pc,COMM.Command[1],COMM.Status[1],Ram_Word_State,main68k_context.sr,sub68k_context.sr,sectors_read); fflush(trace); }
}
static void subcall(void) {
    struct S68000CONTEXT *c=&sub68k_context;
    unsigned fn=c->dreg[0]&0xffff,a=c->areg[0],i;
    unsigned char b[2048]; /* MODE1 payload; EDC/ECC are not program data. */
    if(fn<256&&!sub_seen[fn]++&&trace)fprintf(trace,"sub fn=%02x a0=%x a1=%x d1=%x\n",fn,a,c->areg[1],c->dreg[1]);
    c->sr &= ~1;
    switch(fn) {
    case 0x02: audio_track=-1; audio_seek_delay=0; break;
    case 0x03: audio_paused=1; audio_seek_delay=0; break;
    case 0x04: audio_paused=0; audio_seek_delay=0; break;
    case 0x11: case 0x12: case 0x13: case 0x15: case 0x19: {
        unsigned t=memr(1,a);
        unsigned seek_frames=0;
        if(t<2 || t>(unsigned)SCD.TOC.Last_Track || Tracks[t-1].Type!=TYPE_CDDA) { c->sr|=1; unsupported(1,fn); break; }
        if(fn==0x19) {
            unsigned current=audio_track>=1?Tracks[audio_track].DiscStart+audio_sector:read_lba;
            unsigned target=Tracks[t-1].DiscStart;
            unsigned distance=current>target?current-target:target-current;
            /* Use the image drive's existing CDD seek approximation (20
             * ticks plus distance/4096 at 75 Hz), converted to video frames.
             * Clients can finish their palette fade while the drive seeks. */
            seek_frames=((20+(distance>>12))*(CPU_Mode?50:60)+74)/75;
        }
        /* MSCSEEK1 exposes seeking-for-play before playing once.
         * Keep it visible across a frame so callers can observe CDBSTAT. */
        audio_track=t-1; audio_sector=audio_clock=audio_paused=0;
        audio_seek_delay=seek_frames;
        if(fn==0x15 || fn==0x19)audio_paused=1;
        audio_mode=fn==0x19?0x12:fn; CD_Audio_Starting=1;
        if(trace)fprintf(trace,"CDDA track %u started\n",t);
        break;
    }
    case 0x08: read_paused=1; break;
    case 0x09: read_paused=0; break;
    case 0x18: read_lba=meml(1,a); read_count=0; read_paused=1; audio_track=-1; if(read_lba>=(unsigned)Tracks[0].Lenght)c->sr|=1; break;
    case 0x10: audio_track=-1; read_count=0; break;
    case 0x80: break;
    case 0x84: break; /* CDBPAUSE: image drives do not spin down in standby. */
    case 0x93: case 0x94:
        /* SCDPQ/SCDPQL: no queued Q subcode is available. A MODE1/2352
         * image contains no raw subchannels; report the BIOS read failure
         * without changing the caller's buffer or advancing A0. */
        c->sr|=1;
        break;
    case 0x96: /* CDCSETMODE: HLE transfers sectors through the sub CPU. */
        if(c->dreg[1]&0xffff) { c->sr|=1; unsupported(1,fn); break; }
        read_count=0; read_paused=0; break;
    case 0x81: {
        unsigned state=audio_seek_delay?0x800:
            (audio_paused || (read_count && read_paused))?0x500:
            (audio_track>=1 || read_count)?0x100:0;
        /* CDBSTAT returns a pointer, not a caller-provided output buffer. */
        c->areg[0]=0x1300; prg_clear(0x1300,32);
        prg_ww(0x1300,state|(read_count?(read_paused?5:1):0));
        prg_ww(0x1310,0x100|SCD.TOC.Last_Track);
        prg_wb(0x1307,audio_track>=1?audio_track+1:1);
        /* The CDD table includes BCD absolute/track-relative MSF, not just
         * the track number. Games wait for the relative frame to advance
         * before beginning a transfer (Mega-CD BIOS Manual, pp. 20-21). */
        {
            unsigned relative=audio_track>=1?audio_sector:read_lba;
            unsigned absolute=relative+150+(audio_track>=1?Tracks[audio_track].DiscStart:0);
            unsigned flag=audio_track>=1?0:4;
            prg_wl(0x1308,(bcd(absolute/4500)<<24)|(bcd(absolute/75%60)<<16)|(bcd(absolute%75)<<8)|flag);
            prg_wl(0x130c,(bcd(relative/4500)<<24)|(bcd(relative/75%60)<<16)|(bcd(relative%75)<<8)|flag);
        }
        break;
    }
    case 0x83: { /* CDBTOCREAD: absolute BCD MSF/track and track type. */
        unsigned t=c->dreg[1]&0xffff, pos=0;
        if(t && t<=(unsigned)SCD.TOC.Last_Track)pos=Tracks[t-1].DiscStart;
        else t=0; /* Missing track: disc start, with a zero track byte. */
        pos+=150;
        c->dreg[0]=(bcd(pos/4500)<<24)|(bcd(pos/75%60)<<16)|(bcd(pos%75)<<8)|t;
        c->dreg[1]=(c->dreg[1]&~255U)|(t<=1?255:0);
        break;
    }
    case 0x17: case 0x20: case 0x21:
        transfer_counted=0;
        read_lba=meml(1,a); read_paused=0; audio_track=-1;
        read_count=fn==0x17?(unsigned)Tracks[0].Lenght-read_lba:meml(1,a+4);
        if(fn==0x21)read_count-=read_lba;
        if(trace)fprintf(trace,"READ lba=%u count=%u\n",read_lba,read_count);
        if(read_lba >= (unsigned)Tracks[0].Lenght || read_count > (unsigned)Tracks[0].Lenght-read_lba) { read_count=0; c->sr|=1; }
        break;
    case 0x85:
        i=c->dreg[1]&0x7fff; if(i>1024)i=1024;
        if(c->dreg[1]&0x8000)master_volume=i;
        else { volume=fade_target=i; fade_step=0; }
        break;
    case 0x86:
        fade_target=c->dreg[1]>>16; if(fade_target>1024)fade_target=1024;
        fade_step=c->dreg[1]&0xffff; if(fade_step>1024)fade_step=1024;
        break;
    case 0x87: case 0x88: read_paused=0; break;
    case 0x89: read_count=0; break;
    case 0x8a: if(!read_count||read_paused||!read_budget)c->sr|=1; break;
    case 0x8b:
        if(!read_count||read_paused||!read_budget)c->sr|=1;
        else {
            unsigned h=sector_header(read_lba), dest=CDC.RS0&0x700;
            c->dreg[0]=h;
            if(dest==0x200 || dest==0x300 || dest==0x400 || dest==0x500 || dest==0x700) {
                /* Feed host-port readers and DMA users through the existing CDC
                 * DRQ/EDT machinery. DMA receives payload only, without header. */
                if(!sector(read_lba,CDC.Buffer+4,2048)) { c->sr|=1; break; }
                count_data_sector();
                CDC.Buffer[0]=(unsigned char)(h>>24); CDC.Buffer[1]=(unsigned char)(h>>16);
                CDC.Buffer[2]=(unsigned char)(h>>8); CDC.Buffer[3]=(unsigned char)h;
                CDC.DAC.N=0; CDC.DBC.N=2052;
                if(dest>=0x400) {
                    memmove(CDC.Buffer,CDC.Buffer+4,2048); CDC.DBC.N=2048;
                }
                CDC.RS0=(CDC.RS0&~0xc000)|0x4000;
                CDC.IFSTAT&=~8; SCD.Status_CDC|=8;
            }
        }
        break;
    case 0x8c:
        if(!read_count||read_paused||!read_budget||!sector(read_lba,b,sizeof(b))) { c->sr|=1; break; }
        count_data_sector();
        for(i=0;i<sizeof(b);i+=2)memw(1,a+i,b[i]*256+b[i+1]);
        i=sector_header(read_lba);
        memw(1,c->areg[1],i>>16); memw(1,c->areg[1]+2,i);
        c->areg[0]+=sizeof(b); c->areg[1]+=4;
        break;
    case 0x8d:
        SCD.Status_CDC&=~8; CDC.RS0&=~0x4000;
        if(read_count && !read_paused && read_budget) {
            read_budget--; read_lba++; read_count--; sectors_read++;
            transfer_counted=0;
        }
        break;
    default: unsupported(1,fn); c->sr|=1; break;
    }
    ret(1);
}
static void vram_clear(unsigned start,unsigned bytes) {
    unsigned i;
    Set_VDP_Reg(15,2);
    Write_VDP_Ctrl((unsigned short)(0x4000|(start&0x3fff)));
    Write_VDP_Ctrl((unsigned short)(start>>14));
    for(i=0;i<bytes;i+=2)M68K_Data_WW(0xc00000,0);
}
/* Nemesis tile streams contain a prefix table followed by nibble runs.
 * The six-one prefix encodes an inline run; bit 15 of the tile count selects
 * XOR against the preceding 32-bit row. No firmware code is executed here. */
static int nemesis(unsigned source,unsigned destination,int to_ram) {
    unsigned a=source,header=memr(0,a),tiles=header&0x7fff,pixel=0,n=0,i;
    unsigned char code[256],length[256],runs[256],color[256];
    unsigned bits=0,remaining=0,produced=0,row=0,row_pixels=0,previous=0;
    a+=2;
    for(i=0;i<512;i++) {
        unsigned d=M68K_Data_RB(a++);
        if(d==255)break;
        if(d&128) { pixel=d&15; d=M68K_Data_RB(a++); }
        if(n>=256 || !(d&15) || (d&15)>8)return 0;
        length[n]=d&15; runs[n]=((d>>4)&7)+1; color[n]=pixel; code[n++]=(unsigned char)M68K_Data_RB(a++);
    }
    if(i==512)return 0;
    while(produced<tiles*64) {
        unsigned prefix=0,len,run=0,value=0;
        for(len=1;len<=8;len++) {
            if(!remaining) { bits=M68K_Data_RB(a++); remaining=8; }
            prefix=(prefix<<1)|((bits>>--remaining)&1);
            if(len==6 && prefix==63) {
                unsigned inline_bits=0,j;
                for(j=0;j<7;j++) {
                    if(!remaining) { bits=M68K_Data_RB(a++); remaining=8; }
                    inline_bits=(inline_bits<<1)|((bits>>--remaining)&1);
                }
                run=(inline_bits>>4)+1; value=inline_bits&15; break;
            }
            for(i=0;i<n;i++)if(length[i]==len && code[i]==prefix) { run=runs[i]; value=color[i]; break; }
            if(run)break;
        }
        if(!run || run>tiles*64-produced)return 0;
        while(run--) {
            row=(row<<4)|value; produced++;
            if(++row_pixels==8) {
                unsigned out=(header&0x8000)?row^previous:row;
                previous=out;
                if(to_ram) { memw(0,destination,out>>16); memw(0,destination+2,out); destination+=4; }
                else { M68K_Data_WW(0xc00000,(unsigned short)(out>>16)); M68K_Data_WW(0xc00000,(unsigned short)out); }
                row=row_pixels=0;
            }
        }
    }
    return 1;
}
static void maincall(unsigned fn) {
    struct S68000CONTEXT *c=&main68k_context;
    if(!main_seen[(fn>>2)&255]++&&trace)fprintf(trace,"main fn=%03x a0=%x a1=%x d0=%x d1=%x\n",fn,c->areg[0],c->areg[1],c->dreg[0],c->dreg[1]);
    switch(fn) {
    case 0x338: case 0x33c: { /* BIOS LCG, including the writable seed. */
        unsigned product=(unsigned)((int)(short)main_rw(0xfe2a)*0x3619);
        unsigned sum=(product&0xffff)+0x5d35, seed=sum&0xffff;
        main_ww(0xfe2a,seed);
        if(fn==0x338) {
            unsigned bound=c->dreg[0]&0xffff;
            unsigned scaled, bias=(unsigned)((int)(bound<<16)>>1);
            c->dreg[1]=(unsigned)((int)(short)seed*(int)(short)bound);
            scaled=bias+c->dreg[1];
            c->dreg[0]=(unsigned)(int)(short)(scaled>>16);
            c->sr=(c->sr&~31)|(scaled<bias?16:0)|(c->dreg[0]==0?4:0)|(c->dreg[0]&0x80000000?8:0);
        } else {
            c->dreg[0]=(product&0xffff0000)|seed;
            c->sr=(c->sr&~31)|(seed==0?4:0)|(seed&0x8000?8:0)|
                ((~(product^0x5d35)&(product^seed)&0x8000)?2:0)|(sum>0xffff?17:0);
        }
        break;
    }
    case 0x310: { /* Copy whole longwords, then tail-call A2 (used to replace IP). */
        unsigned n=c->dreg[0]>>2, src=c->areg[0], dst=c->areg[1], i, value=0;
        unsigned count=n+((n&0xffff)?0:65536);
        for(i=0;i<count;i++) { value=meml(0,src); memw(0,dst,value>>16); memw(0,dst+2,value); src+=4; dst+=4; }
        c->areg[0]=src; c->areg[1]=dst;
        c->dreg[0]=(n<<16)|0xffff; c->dreg[1]|=0xffff;
        c->sr=(c->sr&~31)|((n&0xffff)?0:16)|(value==0?4:0)|(value&0x80000000?8:0);
        c->pc=c->areg[2]&0xffffff; c->interrupts[0]&=~0x10;
        return;
    }
    case 0x304: case 0x308:
        main_wb(0xfe26,fn==0x308?3:c->dreg[0]);
        c->sr &= 0xf8ff;
        c->pc=0x1840; c->interrupts[0]&=~0x10;
        return;
    case 0x2ac: {
        static const unsigned char defaults[19]={4,0x24,0x30,0x28,7,0x5c,0,0,0,0,0,0,0x81,0x2f,0,2,0x11,0,0};
        unsigned i;
        for(i=0;i<19;i++) { Set_VDP_Reg(i,defaults[i]); main_ww(0xfdb4+i*2,0x8000+i*0x100+defaults[i]); }
        main_ww(0xfe2e,0x80);
        break;
    }
    case 0x2b0: {
        unsigned i;
        for(i=0;i<32;i++) { unsigned v=memr(0,c->areg[1]); if(!(v&0x8000))break;
            if(((v>>8)&31)<19)main_ww(0xfdb4+((v>>8)&31)*2,v);
            Write_VDP_Ctrl((unsigned short)v); c->areg[1]+=2;
        }
        break;
    }
    case 0x2ec: case 0x2f0: {
        /* Nemesis writes to the already selected VDP address. D0 is saved
         * by the BIOS, not a destination command; RAM output uses A2.
         * Both entry points preserve the input CPU registers. */
        if(!nemesis(c->areg[1],c->areg[2],fn==0x2f0))unsupported(0,fn);
        break;
    }
    case 0x2bc: case 0x2c0: {
        unsigned cmd=c->dreg[0]|0x40000080, n=c->dreg[1], set2=VDP_Reg.Set2;
        Set_VDP_Reg(15,1); Set_VDP_Reg(1,set2|0x10);
        Set_VDP_Reg(19,n&255); Set_VDP_Reg(20,(n>>8)&255); Set_VDP_Reg(23,0x80);
        Write_VDP_Ctrl((unsigned short)(cmd>>16)); Write_VDP_Ctrl((unsigned short)cmd);
        M68K_Data_WW(0xc00000,(unsigned short)(fn==0x2bc?0:(c->dreg[2]&255)*0x101));
        Set_VDP_Reg(1,set2); Set_VDP_Reg(15,2);
        break;
    }
    case 0x2b4: case 0x2b8: {
        unsigned cmd=c->dreg[0],i,n=(c->dreg[1]&0xffff)+1,value=fn==0x2b8?0:c->dreg[2];
        Write_VDP_Ctrl((unsigned short)(cmd>>16)); Write_VDP_Ctrl((unsigned short)cmd);
        for(i=0;i<n;i++)M68K_Data_WW(0xc00000,(unsigned short)value);
        break;
    }
    case 0x30c: {
        unsigned i;
        if(!(main_rb(0xfe26)&1))break;
        Set_VDP_Reg(15,2); Write_VDP_Ctrl(0x7800); Write_VDP_Ctrl(2);
        for(i=0;i<640;i+=2)M68K_Data_WW(0xc00000,(unsigned short)main_rw(0xf900+i));
        break;
    }
    case 0x2c4: case 0x2c8: case 0x2cc: case 0x36c: case 0x334: {
        unsigned y,x,cmd=c->dreg[0],src=c->areg[1],tile=c->dreg[3],cols=(c->dreg[1]&0xffff)+1,rows=(c->dreg[2]&0xffff)+1;
        for(y=0;y<rows;y++) {
            Write_VDP_Ctrl((unsigned short)(cmd>>16)); Write_VDP_Ctrl((unsigned short)cmd);
            for(x=0;x<cols;x++) {
                if(fn==0x2c4) { tile=memr(0,src); src+=2; }
                else if(fn==0x2c8) { tile=(tile&~255U)|M68K_Data_RB(src++); }
                M68K_Data_WW(0xc00000,(unsigned short)(fn==0x334?tile+y+x*rows:tile));
                if(fn==0x36c)tile++;
            }
            cmd+=main_rw(0xfe2e)<<16;
        }
        c->areg[1]=src; c->dreg[0]=cmd;
        break;
    }
    case 0x394: { /* A1: zero-terminated list of word count, VDP command, source. */
        unsigned a=c->areg[1], n, i, inc=main_rw(0xfdd2)&255;
        while((n=memr(0,a))!=0) {
            unsigned cmd=meml(0,a+2)|0x40000000, src=meml(0,a+6);
            a+=10;
            Set_VDP_Reg(15,2);
            Write_VDP_Ctrl((unsigned short)(cmd>>16)); Write_VDP_Ctrl((unsigned short)(cmd&~0x80));
            for(i=0;i<n;i++)M68K_Data_WW(0xc00000,(unsigned short)memr(0,src+i*2));
            Set_VDP_Reg(15,inc);
            c->dreg[0]=cmd; c->dreg[1]=src;
        }
        c->dreg[2]&=0xffff0000; c->areg[1]=a+2;
        break;
    }
    case 0x2d0: case 0x2d4: {
        unsigned cmd=c->dreg[0]|0x40000000, src=c->dreg[1], n=c->dreg[2]&0xffff, i, inc=main_rw(0xfdd2)&255;
        if(!n)n=65536;
        if(fn==0x2d4)Set_VDP_Reg(15,2);
        Write_VDP_Ctrl((unsigned short)(cmd>>16)); Write_VDP_Ctrl((unsigned short)(cmd&~0x80));
        for(i=0;i<n;i++)M68K_Data_WW(0xc00000,(unsigned short)memr(0,src+i*2));
        if(fn==0x2d4)Set_VDP_Reg(15,inc);
        break;
    }
    case 0x2d8: main_ww(0xfdb6,main_rw(0xfdb6)|0x40); Set_VDP_Reg(1,VDP_Reg.Set2|0x40); break;
    case 0x2dc: main_ww(0xfdb6,main_rw(0xfdb6)&~0x40); Set_VDP_Reg(1,VDP_Reg.Set2&~0x40); break;
    case 0x2e0: case 0x2e4: {
        unsigned a=c->areg[1], header=memr(0,a), offset=header>>8, count=(header&255)+1, i;
        if(fn==0x2e4)main_wb(0xfe29,main_rb(0xfe29)|1);
        for(i=0;i<count;i++)memw(0,0xfffb80+offset+i*2,memr(0,a+2+i*2));
        c->areg[1]=a+2+count*2; c->dreg[0]=0xffff;
        break;
    }
    case 0x2e8: { unsigned i; if(!(main_rb(0xfe29)&1))break; main_wb(0xfe29,main_rb(0xfe29)&~1); for(i=0;i<128;i+=2)ww(CRam,i,main_rw(0xfb80+i)); CRam_Flag=1; VDPTransfer_RecordHLEWrite(VDP_TRANSFER_CRAM,128); } break;
    case 0x29c: {
        unsigned port=c->areg[6]&0xffffff, hi,lo;
        M68K_Data_WB(port+6,0x40); M68K_Data_WB(port,0x40); hi=M68K_Data_RB(port);
        M68K_Data_WB(port,0); lo=M68K_Data_RB(port);
        c->dreg[7]=((hi&12)?8:0)|((hi&3)?4:0)|((lo&12)?2:0)|((lo&3)?1:0);
        break;
    }
    case 0x298: {
        unsigned port;
        for(port=0;port<2;port++) {
            unsigned addr=0xa10003+port*2,low,high,pressed,old=main_rb(0xfe20+port*2);
            M68K_Data_WB(addr,0); low=M68K_Data_RB(addr); M68K_Data_WB(addr,0x40); high=M68K_Data_RB(addr);
            pressed=~(((low<<2)&0xc0)|(high&0x3f))&255;
            main_ww(0xfe20+port*2,(pressed<<8)|(pressed&~old));
        }
        break;
    }
    case 0x2a0: vram_clear(0,65536); memset(VSRam,0,80); ww(CRam,0,0); CRam_Flag=1; VDPTransfer_RecordHLEWrite(VDP_TRANSFER_VSRAM,80); VDPTransfer_RecordHLEWrite(VDP_TRANSFER_CRAM,2); break;
    case 0x2a4: vram_clear(0xa000,0xe00); vram_clear(0xb800,4); vram_clear(0xc000,0x4000); main_wl(0xf900,0); break;
    case 0x2a8: memset(VSRam,0,80); VDPTransfer_RecordHLEWrite(VDP_TRANSFER_VSRAM,80); break;
    case 0x368: main_wl(0xfd08,c->areg[1]); break;
    case 0x348: { /* Decode the sub CPU's buffered disc status after exchange. */
        unsigned flags=main_rb(0xfdde), subflags=main_rb(0xfddf), status;
        M68K_Data_WB(0xa1200e,M68K_Data_RB(0xa1200e)&~2);
        if(!(flags&2) || !(subflags&4))break;
        main_wb(0xfdde,main_rb(0xfdde)|4);
        status=main_rw(0xfdf4); main_ww(0xfe3a,status);
        if(status&0xa000)break;
        main_wb(0xfe57,main_rb(0xfdfb));
        if(subflags&8) {
            if(!(flags&0x20))break;
            main_ww(0xfe3c,main_rw(0xfdf6));
            main_ww(0xfe3e,main_rw(0xfdfc));
            main_wb(0xfe43,main_rb(0xfdfe));
            main_wb(0xfdde,main_rb(0xfdde)|0x40);
        } else {
            main_ww(0xfe44,main_rw(0xfdf6));
            main_ww(0xfe40,main_rw(0xfdfc));
            main_wb(0xfe42,main_rb(0xfdfe));
            main_wb(0xfdde,main_rb(0xfdde)|0x20);
        }
        break;
    }
    case 0x350: case 0x354: case 0x358: { /* Queue a pair of words in a communication slot. */
        unsigned offset=fn==0x350?0:fn==0x354?8:12;
        main_ww(0xfde0+offset,c->dreg[0]);
        main_ww(0xfde2+offset,c->dreg[1]);
        break;
    }
    case 0x35c: /* Queue the system command in the second communication slot. */
        if((c->dreg[0]&0xffff)==1)M68K_Data_WB(0xa1200e,M68K_Data_RB(0xa1200e)|4);
        main_ww(0xfde4,c->dreg[0]);
        main_ww(0xfde6,c->dreg[1]);
        break;
    case 0x344: { /* Exchange buffered commands with the sub CPU. */
        unsigned flags=M68K_Data_RB(0xa1200e);
        M68K_Data_WB(0xa1200e,flags|1);
        main_wb(0xfdde,main_rb(0xfdde)&~2);
        if((S68K_State&3)!=1)break;
        if(M68K_Data_RB(0xa1200f)&1) {
            main_wb(0xfdde,main_rb(0xfdde)^1); main_wb(0xfdde,main_rb(0xfdde)|2);
            comm_wait=1; return;
        }
        sub68k_interrupt(2,-1); M68K_Data_WB(0xa1200e,flags|3);
        break;
    }

    case 0x378: { /* Queue a BIOS CD command for the next communication exchange. */
        static const unsigned char codes[]={0x0a,2,3,4,5,6,7,8,9,0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x20,0x21,0x80,0x81,0x82,0x83,0x85,0x86,0x87,0x89,0x8a,0x8b,0x8c,0x8d,0x8f,0x90,0x91,0x92,0x93,0,0xc0,0xc1,0x19,0xc2,0xc3};
        /* Parameter layouts: none, packed byte, packed volume, or word. */
        static const unsigned char formats[]={0,0,0,0,0,0,0,0,0,3,3,3,3,1,3,1,1,1,3,3,0,0,3,3,2,2,0,0,0,0,3,0,0,0,0,3,3,3,3,3,3,3,3};
        unsigned index=(c->dreg[0]&0xffff)/2, code, value=c->dreg[1]&0xffff;
        if(index>=sizeof(codes)) { unsupported(0,fn); break; }
        code=codes[index];
        if(formats[index]==0)main_ww(0xfde0,code);
        else if(formats[index]==1)main_wl(0xfde0,((value&0xff00)|code)<<16|(c->dreg[1]>>16));
        else {
            if(formats[index]==2) {
                value|=(c->dreg[2]&15)<<12; code|=(c->dreg[2]&0xff0)<<4;
            }
            main_wl(0xfde0,(code<<16)|value);
        }
        break;
    }
    case 0x340: { /* CLEAR_COMM: clear the main CPU-owned communication registers. */
        unsigned i; M68K_Data_WB(0xa1200e,0);
        for(i=0;i<16;i+=2)M68K_Data_WW(0xa12010+i,0);
        main_clear(0xfdde,18);
        break;
    }
    case 0x360: if((c->sr&0x700)==0x600)native_irq2=1; sub68k_interrupt(2,-1); break;
    case 0x364: break; /* Boot presentation is intentionally omitted. */
    default: unsupported(0,fn); break;
    }
    ret(0);
}
unsigned CD_HLE_MainExec(int cycles) {
    unsigned result, previous_irq=sub68k_context.interrupts[0]&4;
    unsigned in_vblank=(main68k_context.sr&0x700)==0x600;
    if(!CD_HLE_Active)return M68K_Exec_Profiled(cycles);
    if(Paused)return 0x80000004;
    if(comm_wait) {
        unsigned i, flags=M68K_Data_RB(0xa1200f);
        /* The sub CPU acknowledges the phase before either side copies data.
         * Yield this CPU slice while it waits, so the sub CPU can run. */
        if(((flags>>1)&1)!=(main_rb(0xfdde)&1))return 0;
        main_wb(0xfddf,flags);
        for(i=0;i<16;i+=2) {
            M68K_Data_WW(0xa12010+i,main_rw(0xfde0+i));
            main_ww(0xfde0+i,0);
            main_ww(0xfdf0+i,M68K_Data_RW(0xa12020+i));
        }
        sub68k_interrupt(2,-1);
        M68K_Data_WB(0xa1200e,M68K_Data_RB(0xa1200e)|2);
        comm_wait=0; ret(0);
    }
    result=M68K_Exec_Profiled(cycles);
    if(in_vblank && !previous_irq && (sub68k_context.interrupts[0]&4))native_irq2=1;
    if((main68k_context.interrupts[0]&0x10) && main68k_context.pc>=0x2006 && main68k_context.pc<0x2400 && (main68k_context.pc&7)==6) {
        unsigned fn=0x200+(main68k_context.pc-0x2006)/2;
        restore_sr(0); maincall(fn);
    }
    return result;
}
unsigned CD_HLE_SubExec(int cycles) {
    unsigned result;
    if(CD_HLE_Active && Paused)return 0x80000004;
    result=S68K_Exec_Profiled(cycles);
    if(CD_HLE_Active && (sub68k_context.interrupts[0]&1) && sub68k_context.pc==0x1206) { restore_sr(1); subcall(); }
    else if(CD_HLE_Active && (sub68k_context.interrupts[0]&1) && sub68k_context.pc==0x1216) { restore_sr(1); bramcall(); }
    return result;
}
