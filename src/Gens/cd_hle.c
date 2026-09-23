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
#include "Cpu_68k.h"
#include "Mem_M68K.h"
#include "Mem_S68K.h"
#include "Mem_Z80.h"
#include "cd_file.h"
#include "cd_sys.h"
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
static unsigned frame;
static int audio_track=-1, audio_sector, audio_mode, audio_paused, audio_clock;
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
static unsigned sector_header(unsigned lba) {
    unsigned t=lba+150,m=t/4500,s=t/75%60,f=t%75;
    return ((m/10*16+m%10)<<24)|((s/10*16+s%10)<<16)|((f/10*16+f%10)<<8)|1;
}
static int sector(unsigned lba, unsigned char *b, unsigned bytes) {
    unsigned stride=Tracks[0].Type==TYPE_BIN?2352:2048;
    if(!Tracks[0].F || Tracks[0].Lenght<=0 || lba >= (unsigned)Tracks[0].Lenght || lba>0x7fffffffU/stride)return 0;
    if(fseek(Tracks[0].F,lba*stride+(stride==2352?16:0),SEEK_SET))return 0;
    if(stride==2048 && bytes>2048) { memset(b+2048,0,bytes-2048); bytes=2048; }
    return fread(b,1,bytes,Tracks[0].F)==bytes;
}
static unsigned memr(int sub,unsigned a) {
    a &= 0xffffff;
    if(sub) { if(a<0x80000)return rw(Ram_Prg,a); return S68K_RW(a); }
    if(a>=0xff0000)return rw(Ram_68k,a&0xffff);
    if(a<0x20000)return rw(Rom_Data,a);
    return M68K_RW(a);
}
static void memw(int sub,unsigned a,unsigned v) {
    a &= 0xffffff;
    if(sub) { if(a<0x80000)ww(Ram_Prg,a,v); else S68K_WW(a,(unsigned short)v); }
    else { if(a>=0xff0000)ww(Ram_68k,a&0xffff,v); else M68K_WW(a,(unsigned short)v); }
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
            copy_be(Ram_Prg,dest,saves[found].data,n);
        } else if(fn==8) {
            unsigned n=saves[found].blocks*(saves[found].mode?32:64);
            if(dest>=0x80000 || n>0x80000-dest) { c->sr|=1; break; }
            c->dreg[0]=0;
            for(j=0;j<n;j++)if(Ram_Prg[(dest+j)^1]!=saves[found].data[j]) { c->sr|=1; c->dreg[0]=0xffffffff; break; }
        }
        break;
    case 4: {
        unsigned blocks=memr(1,a+12),mode=memr(1,a+10)&255,n=blocks*(mode?32:64);
        int slot=found>=0?found:empty;
        if(slot<0||!blocks||blocks>125||n>8000||dest>=0x80000||n>0x80000-dest||used-(found>=0?saves[found].blocks:0)+blocks>125) { c->sr|=1; break; }
        memcpy(saves[slot].name,name,11); saves[slot].mode=(unsigned char)mode; saves[slot].blocks=blocks;
        for(j=0;j<n;j++)saves[slot].data[j]=Ram_Prg[(dest+j)^1];
        if(!write_saves())c->sr|=1;
        break;
    }
    case 5: if(found<0)c->sr|=1; else { saves[found].blocks=0; if(!write_saves())c->sr|=1; } break;
    case 6: memset(saves,0,sizeof(saves)); if(!write_saves())c->sr|=1; else saves_bad=0; break;
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
    read_lba=read_count=frame=0; read_paused=0;
    audio_track=-1; audio_sector=audio_clock=audio_paused=0;
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
    ww(Rom_Data,0x1800,0x4238); ww(Rom_Data,0x1802,0xfe26); ww(Rom_Data,0x1804,0x4e73);
    /* Wait for the game's V-blank handler to acknowledge the BIOS work flag. */
    ww(Rom_Data,0x1840,0x4a38); ww(Rom_Data,0x1842,0xfe26);
    ww(Rom_Data,0x1844,0x66fa); ww(Rom_Data,0x1846,0x4e75);
    wl(Rom_Data,0x78,0xfffd06); ww(Ram_68k,0xfd06,0x4ef9); wl(Ram_68k,0xfd08,0x1800);
    wl(Rom_Data,0x70,0xfffd0c); ww(Ram_68k,0xfd0c,0x4ef9); wl(Ram_68k,0xfd0e,0x1800);
    /* Sub-program initialization followed by its main entry point. */
    ww(Ram_Prg,0x1000,0x4eb9); wl(Ram_Prg,0x1002,init);
    ww(Ram_Prg,0x1006,0x4ef9); wl(Ram_Prg,0x1008,main);
    for(i=8;i<0x100;i+=4)wl(Ram_Prg,i,0x1100);
    ww(Ram_Prg,0x1100,0x4e73);
    wl(Ram_Prg,0x68,0x1120);
    ww(Ram_Prg,0x1120,0x48e7); ww(Ram_Prg,0x1122,0xfffe);
    ww(Ram_Prg,0x1124,0x4eb9); wl(Ram_Prg,0x1126,irq);
    ww(Ram_Prg,0x112a,0x4cdf); ww(Ram_Prg,0x112c,0x7fff); ww(Ram_Prg,0x112e,0x4e73);
    stub(Ram_Prg,0x5f22,0x1200); stub(Ram_Prg,0x5f16,0x1210);
    main68k_context.pc=0xff0000; main68k_context.areg[7]=0xfffd00; main68k_context.sr=0x2000;
    sub68k_context.pc=0x1000; sub68k_context.areg[7]=0x5e00; sub68k_context.sr=0x2000;
    S68K_State=1; Int_Mask_S68K=4; Ram_Word_State=0; MS68K_Set_Word_Ram();
    Set_VDP_Reg(1,0x24); Set_VDP_Reg(15,2);
    if(trace)fprintf(trace,"boot ip=%x/%x sp=%x/%x init=%x main=%x irq=%x\n",ip_off,ip_size,sp_off,sp_size,init,main,irq);
    return 1;
}

int CD_HLE_AudioPlaying(void) { return CD_HLE_Active && audio_track>=1 && !audio_paused; }

static void audio_tick(void) {
    short samples[1176];
    int clock_rate=Sound_Rate>0?Sound_Rate:44100;
    int frame_samples=Seg_Lenght>0?Seg_Lenght:clock_rate/(CPU_Mode?50:60);
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
            else { audio_track=-1; break; }
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
    audio_tick();
    sub68k_interrupt(2,-1);
    if(trace && frame%60==0) { fprintf(trace,"frame %u main=%06x sub=%06x comm=%04x/%04x word=%u sr=%x/%x\n",frame,main68k_context.pc,sub68k_context.pc,COMM.Command[1],COMM.Status[1],Ram_Word_State,main68k_context.sr,sub68k_context.sr); fflush(trace); }
}
static void subcall(void) {
    struct S68000CONTEXT *c=&sub68k_context;
    unsigned fn=c->dreg[0]&0xffff,a=c->areg[0],i;
    unsigned char b[2336];
    if(fn<256&&!sub_seen[fn]++&&trace)fprintf(trace,"sub fn=%02x a0=%x a1=%x d1=%x\n",fn,a,c->areg[1],c->dreg[1]);
    c->sr &= ~1;
    switch(fn) {
    case 0x02: audio_track=-1; break;
    case 0x03: audio_paused=1; break;
    case 0x04: audio_paused=0; break;
    case 0x11: case 0x12: case 0x13: {
        unsigned t=memr(1,a);
        if(t<2 || t>(unsigned)SCD.TOC.Last_Track || Tracks[t-1].Type!=TYPE_CDDA) { c->sr|=1; unsupported(1,fn); break; }
        audio_track=t-1; audio_sector=audio_clock=audio_paused=0; audio_mode=fn; CD_Audio_Starting=1;
        if(trace)fprintf(trace,"CDDA track %u started\n",t);
        break;
    }
    case 0x08: read_paused=1; break;
    case 0x09: read_paused=0; break;
    case 0x18: read_lba=meml(1,a); read_count=0; read_paused=1; audio_track=-1; if(read_lba>=(unsigned)Tracks[0].Lenght)c->sr|=1; break;
    case 0x10: audio_track=-1; read_count=0; break;
    case 0x80: break;
    case 0x81:
        /* CDBSTAT returns a pointer, not a caller-provided output buffer. */
        c->areg[0]=0x1300; memset(Ram_Prg+0x1300,0,32);
        ww(Ram_Prg,0x1300,(audio_track>=1?(audio_paused?0x500:0x100):0)|(read_count?(read_paused?5:1):0));
        break;
    case 0x17: case 0x20: case 0x21:
        read_lba=meml(1,a); read_paused=0; audio_track=-1;
        read_count=fn==0x17?(unsigned)Tracks[0].Lenght-read_lba:meml(1,a+4);
        if(fn==0x21)read_count-=read_lba;
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
    case 0x8a: if(!read_count||read_paused)c->sr|=1; break;
    case 0x8b: if(!read_count||read_paused)c->sr|=1; else c->dreg[0]=sector_header(read_lba); break;
    case 0x8c:
        if(!read_count||read_paused||!sector(read_lba,b,sizeof(b))) { c->sr|=1; break; }
        for(i=0;i<sizeof(b);i+=2)memw(1,a+i,b[i]*256+b[i+1]);
        i=sector_header(read_lba);
        memw(1,c->areg[1],i>>16); memw(1,c->areg[1]+2,i);
        c->areg[0]+=sizeof(b); c->areg[1]+=4;
        break;
    case 0x8d: if(read_count) { read_lba++; read_count--; } break;
    default: unsupported(1,fn); c->sr|=1; break;
    }
    ret(1);
}
static void vram_clear(unsigned start,unsigned bytes) {
    unsigned i;
    Set_VDP_Reg(15,2);
    Write_VDP_Ctrl((unsigned short)(0x4000|(start&0x3fff)));
    Write_VDP_Ctrl((unsigned short)(start>>14));
    for(i=0;i<bytes;i+=2)Write_Word_VDP_Data(0);
}
static void maincall(unsigned fn) {
    struct S68000CONTEXT *c=&main68k_context;
    if(!main_seen[(fn>>2)&255]++&&trace)fprintf(trace,"main fn=%03x a0=%x a1=%x d0=%x d1=%x\n",fn,c->areg[0],c->areg[1],c->dreg[0],c->dreg[1]);
    switch(fn) {
    case 0x304: case 0x308:
        Ram_68k[0xfe26^1]=(unsigned char)(fn==0x308?3:c->dreg[0]);
        c->sr &= 0xf8ff;
        c->pc=0x1840; c->interrupts[0]&=~0x10;
        return;
    case 0x2ac: {
        static const unsigned char defaults[19]={4,0x24,0x30,0x28,7,0x5c,0,0,0,0,0,0,0x81,0x2f,0,2,0x11,0,0};
        unsigned i;
        for(i=0;i<19;i++) { Set_VDP_Reg(i,defaults[i]); ww(Ram_68k,0xfdb4+i*2,0x8000+i*0x100+defaults[i]); }
        ww(Ram_68k,0xfe2e,0x80);
        break;
    }
    case 0x2b0: {
        unsigned i;
        for(i=0;i<32;i++) { unsigned v=memr(0,c->areg[1]); if(!(v&0x8000))break;
            if(((v>>8)&31)<19)ww(Ram_68k,0xfdb4+((v>>8)&31)*2,v);
            Write_VDP_Ctrl((unsigned short)v); c->areg[1]+=2;
        }
        break;
    }
    case 0x2d8: ww(Ram_68k,0xfdb6,rw(Ram_68k,0xfdb6)|0x40); Set_VDP_Reg(1,VDP_Reg.Set2|0x40); break;
    case 0x2dc: ww(Ram_68k,0xfdb6,rw(Ram_68k,0xfdb6)&~0x40); Set_VDP_Reg(1,VDP_Reg.Set2&~0x40); break;
    case 0x2e8: { unsigned i; if(!(Ram_68k[0xfe29^1]&1))break; Ram_68k[0xfe29^1]&=~1; for(i=0;i<128;i+=2)ww(CRam,i,rw(Ram_68k,0xfb80+i)); CRam_Flag=1; } break;
    case 0x298: {
        unsigned port;
        for(port=0;port<2;port++) {
            unsigned addr=0xa10003+port*2,low,high,pressed,old=Ram_68k[(0xfe20+port*2)^1];
            M68K_WB(addr,0); low=M68K_RB(addr); M68K_WB(addr,0x40); high=M68K_RB(addr);
            pressed=~(((low<<2)&0xc0)|(high&0x3f))&255;
            ww(Ram_68k,0xfe20+port*2,(pressed<<8)|(pressed&~old));
        }
        break;
    }
    case 0x2a0: vram_clear(0,65536); memset(VSRam,0,80); ww(CRam,0,0); CRam_Flag=1; break;
    case 0x2a4: vram_clear(0xa000,0xe00); vram_clear(0xb800,4); vram_clear(0xc000,0x4000); wl(Ram_68k,0xf900,0); break;
    case 0x2a8: memset(VSRam,0,80); break;
    case 0x368: wl(Ram_68k,0xfd08,c->areg[1]); break;
    case 0x360: sub68k_interrupt(2,-1); break;
    case 0x364: break; /* Boot presentation is intentionally omitted. */
    default: unsupported(0,fn); break;
    }
    ret(0);
}
unsigned CD_HLE_MainExec(int cycles) {
    unsigned result;
    if(!CD_HLE_Active)return main68k_exec(cycles);
    if(Paused)return 0x80000004;
    result=main68k_exec(cycles);
    if((main68k_context.interrupts[0]&0x10) && main68k_context.pc>=0x2006 && main68k_context.pc<0x2400 && (main68k_context.pc&7)==6) {
        unsigned fn=0x200+(main68k_context.pc-0x2006)/2;
        restore_sr(0); maincall(fn);
    }
    return result;
}
unsigned CD_HLE_SubExec(int cycles) {
    unsigned result;
    if(CD_HLE_Active && Paused)return 0x80000004;
    result=sub68k_exec(cycles);
    if(CD_HLE_Active && (sub68k_context.interrupts[0]&1) && sub68k_context.pc==0x1206) { restore_sr(1); subcall(); }
    else if(CD_HLE_Active && (sub68k_context.interrupts[0]&1) && sub68k_context.pc==0x1216) { restore_sr(1); bramcall(); }
    return result;
}

