// Integration test for the user-supplied FHB disc. No game or BIOS data is embedded.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../src/Gens/gens.h"
#include "../src/Gens/G_main.h"
#include "../src/Gens/Cpu_68k.h"
#include "../src/Gens/Cpu_Z80.h"
#include "../src/Gens/Mem_M68K.h"
#include "../src/Gens/Mem_S68K.h"
#include "../src/Gens/cd_file.h"
#include "../src/Gens/cd_hle.h"
#include "../src/Gens/cd_sys.h"
#include "../src/Gens/vdp_io.h"
#include "../src/Gens/G_dsound.h"
#include "../src/Gens/rom.h"
#include "../src/Gens/save.h"
#include "../src/Gens/vdp_rend.h"
#include "../src/Gens/io.h"

#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAILED line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static int pulse(int frame,int when) { return frame>=when && frame<when+6; }
static int rotate_tests(void)
{
    // Real 68000 instructions, independent of the game's decompressor.
    const unsigned values[]={0,1,0x80,0x8000,0x80000000,0xffffffff,0x81234567};
    for(int sub=0;sub<2;sub++) {
        S68000CONTEXT *c=sub?&sub68k_context:&main68k_context, old=*c;
        unsigned char *ram=sub?Ram_Prg:Ram_68k;
        const unsigned offset=0x1000, pc=sub?offset:0xff0000+offset;
        for(unsigned size=0;size<3;size++)for(unsigned left=0;left<2;left++)
        for(unsigned reg=0;reg<2;reg++)for(unsigned count=reg?0:1;count<=(reg?63:8);count++)
        for(unsigned x=0;x<2;x++)for(unsigned j=0;j<sizeof(values)/sizeof(values[0]);j++) {
            unsigned bits=8u<<size, mask=0xffffffffu>>(32-bits);
            unsigned result=values[j]&mask, extend=x;
            for(unsigned n=0;n<count;n++) {
                unsigned next=left?(result>>(bits-1)):(result&1);
                result=left?((result<<1)|extend)&mask:(result>>1)|(extend<<(bits-1));
                extend=next;
            }
            unsigned flags=(extend?0x11:0)|(result?0:4)|((result>>(bits-1))?8:0);
            unsigned opcode=0xe010|(left<<8)|(size<<6)|(reg?0x220:((count&7)<<9));
            ram[offset]=(unsigned char)opcode;ram[offset+1]=(unsigned char)(opcode>>8);
            *c=old;c->pc=pc;c->sr=(unsigned short)(0x270f|(x<<4));c->odometer=0;
            c->dreg[0]=values[j];c->dreg[1]=count;
            if(sub)sub68k_exec(1);else main68k_exec(1);
            unsigned expected=(values[j]&~mask)|result;
            if(c->pc!=pc+2 || c->dreg[0]!=expected || (c->sr&0x1f)!=flags) {
                fprintf(stderr,"ROX failure: sub=%d opcode=%04x count=%u input=%08x X=%u result=%08x/%08x flags=%02x/%02x\n",
                    sub,opcode,count,values[j],x,c->dreg[0],expected,c->sr&0x1f,flags);
                return 0;
            }
        }
        *c=old;
    }
    puts("PASS: ROXL/ROXR on both CPUs, all widths and counts, result and CCR");
    return 1;
}
static void word(unsigned at,unsigned value) { Ram_Prg[at^1]=(unsigned char)(value>>8); Ram_Prg[(at+1)^1]=(unsigned char)value; }
static unsigned bram(unsigned fn,unsigned a0,unsigned a1,unsigned *d0)
{
    // Exercise the actual CPU-to-host trap, SR restoration, carry and RTS path.
    S68000CONTEXT old=sub68k_context;
    sub68k_context.pc=0x1216; sub68k_context.interrupts[0]=1;
    sub68k_context.areg[7]=0x5800; sub68k_context.areg[0]=a0; sub68k_context.areg[1]=a1;
    sub68k_context.dreg[0]=fn; sub68k_context.sr=0x2700;
    word(0x5800,0x2010); word(0x5802,0); word(0x5804,0x1400);
    CD_HLE_SubExec((int)sub68k_context.odometer);
    unsigned sr=sub68k_context.sr;
    *d0=sub68k_context.dreg[0];
    if(sub68k_context.pc!=0x1400 || sub68k_context.areg[7]!=0x5806 ||
       (sub68k_context.interrupts[0]&1) || (sr&0xfffe)!=0x2010)sr=0xffff;
    sub68k_context=old;
    return sr;
}
static int screenshot(const char *path)
{
    BITMAPFILEHEADER file={0}; BITMAPINFOHEADER info={0};
    file.bfType=0x4d42; file.bfOffBits=sizeof(file)+sizeof(info);
    file.bfSize=file.bfOffBits+336*240*3;
    info.biSize=sizeof(info); info.biWidth=336; info.biHeight=240;
    info.biPlanes=1; info.biBitCount=24;
    FILE *f=fopen(path,"wb"); if(!f)return 0;
    fwrite(&file,1,sizeof(file),f); fwrite(&info,1,sizeof(info),f);
    unsigned lit=0;
    for(int y=239;y>=0;y--)for(int x=0;x<336;x++) {
        unsigned v=MD_Screen[y*336+x];
        unsigned char rgb[3]={(unsigned char)((v&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v>>11)*255/31)};
        fwrite(rgb,1,3,f); if(v)lit++;
    }
    int ok=!ferror(f) && lit>4000; fclose(f); return ok;
}
int main(int argc,char **argv)
{
    CHECK(argc>=2 && argc<=5);
    const int opening=argc>=3 && !strcmp(argv[2],"opening");
    const int resume=argc>=3 && !strcmp(argv[2],"continue");
    const int accurate=argc>=4 && !strcmp(argv[3],"accurate");
    GetCurrentDirectory(sizeof(Gens_Path)-2,Gens_Path);
    strcat(Gens_Path,"\\");
    _putenv("GENS_HLE_TRACE=trace.log");
    _putenv("GENS_HLE_SAVE_DIR=.");
    // BIOS names are deliberately empty. No development opt-in is used.
    JA_CD_Bios[0]=US_CD_Bios[0]=EU_CD_Bios[0]=0;
    RMax_Level=GMax_Level=BMax_Level=224; Contrast_Level=Brightness_Level=100;
    Recalculate_Palettes();
    Sound_Rate=argc>=5?atoi(argv[4]):44100;
    CHECK(Sound_Rate==11025 || Sound_Rate==22050 || Sound_Rate==44100);
    Sound_Enable=0; Country=0; CDDA_Enable=1;
    Seg_Lenght=(Sound_Rate+30)/60;
    for(int i=0;i<262;i++) {
        Sound_Extrapol[i][0]=Seg_Lenght*i/262;
        Sound_Extrapol[i][1]=Seg_Lenght*(i+1)/262-Sound_Extrapol[i][0];
    }
    for(int i=0;i<Seg_Lenght;i++)Sound_Interpol[i]=262*i/Seg_Lenght;
    Z80_State=1; // Match the normal application: Z80 emulation enabled.
    M68K_Init(); S68K_Init(); Z80_Init(); Init_Tab(); FILE_Init();
    CHECK(rotate_tests());
    CHECK(Detect_Format(argv[1])==SEGACD_IMAGE+1);
    SegaCD_Started=Init_SegaCD(argv[1]);
    CHECK(SegaCD_Started && CD_HLE_Active);
    CHECK(SCD.TOC.Last_Track==5 && Tracks[1].Type==TYPE_CDDA);
    CHECK(Tracks[1].DiscStart-Tracks[1].FileOffset==150);
    unsigned audio_frames=0;
    for(int i=0;i<(opening?10500:9000);i++) {
        Controller_1_Up=Controller_1_Down=Controller_1_Left=Controller_1_Right=1;
        Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_C=1;
        Controller_1_Start=(pulse(i,1800)||pulse(i,2400)||(!resume&&pulse(i,5000)))?0:1;
        if(resume) {
            Controller_1_Down=pulse(i,2300)?0:1;
            Controller_1_A=(pulse(i,3000)||pulse(i,3600))?0:1;
        } else {
            Controller_1_A=(pulse(i,4200)||pulse(i,4400)||pulse(i,5600)||pulse(i,6500)||pulse(i,7000)||pulse(i,7500)||pulse(i,8000))?0:1;
            Controller_1_Right=(i>=5400&&i<5430)?0:1;
            Controller_1_Down=(i>=5800&&i<6300&&i%100<6)?0:1;
            Controller_1_B=pulse(i,8500)?0:1;
        }
        if(opening)Controller_1_Start=Controller_1_A=Controller_1_B=Controller_1_Down=Controller_1_Right=1;
        memset(Seg_L,0,sizeof(Seg_L)); memset(Seg_R,0,sizeof(Seg_R));
        const int audio_was_playing=CD_HLE_AudioPlaying();
        const int audio_read_before=CD_Audio_Buffer_Read_Pos;
        if(accurate)Do_SegaCD_Frame_Cycle_Accurate(); else Do_SegaCD_Frame();
        CHECK(!Paused);
        if(audio_was_playing && CD_HLE_AudioPlaying() && !CD_Audio_Starting) {
            const int consumed=(CD_Audio_Buffer_Read_Pos-audio_read_before)&4095;
            if(consumed!=Seg_Lenght) {
                fprintf(stderr,"CD audio discontinuity: frame=%d rate=%d consumed=%d expected=%d\n",i,Sound_Rate,consumed,Seg_Lenght);
                return 1;
            }
        }
        if(i%100==0 && !HeapValidate(GetProcessHeap(),0,NULL)) { fprintf(stderr,"heap corrupted at frame %d\n",i);return 1; }
        if(CD_HLE_AudioPlaying())for(int j=0;j<Seg_Lenght;j++)if(Seg_L[j]||Seg_R[j]) { audio_frames++; break; }
        if(opening) {
            // Sample several scenes, then the title reached without input.
            if(i==899 || i==2099 || i==2999 || i==4199 || i==7799 || i==9899 || i==10499) {
                char name[80];sprintf(name,"opening-%05d.bmp",i+1);
                CHECK(screenshot(name));
            }
            continue;
        }
        if(i==2399)CHECK(screenshot("title.bmp"));
        if(!resume && i==5399)CHECK(screenshot("room-before-move.bmp"));
        if(!resume && i==5499)CHECK(screenshot("room-after-move.bmp"));
        if(!resume && i==7699)CHECK(screenshot("save.bmp"));
    }
    if(opening) { CHECK(audio_frames>3000); Free_Rom(Game); FILE_End(); puts("PASS: full opening and title, with CD audio"); return 0; }
    CHECK(screenshot(resume?"continue.bmp":"gameplay.bmp"));
    CHECK(audio_frames>30);
    CHECK(!Save_State("unsupported-state.gs0"));
    CHECK(GetFileAttributesA("unsupported-state.gs0")==INVALID_FILE_ATTRIBUTES);
    CHECK(!Load_State("unsupported-state.gs0"));
    unsigned size, sr;
    sr=bram(0,0x5808,0x5900,&size); CHECK(sr==0x2010 && size==8192);
    sr=bram(1,0x5808,0x5900,&size); CHECK(sr==0x2010 && size<125);
    // Verify the on-disk game save against the read/verify BIOS API.
    WIN32_FIND_DATAA found;
    HANDLE search=FindFirstFileA("segacd-*.hbr",&found);
    CHECK(search!=INVALID_HANDLE_VALUE); FindClose(search);
    FILE *f=fopen(found.cFileName,"rb"); CHECK(f);
    unsigned char header[24]; CHECK(fread(header,1,24,f)==24); fclose(f);
    CHECK(!memcmp(header,"GensHLE1",8));
    for(unsigned j=0;j<14;j++)Ram_Prg[(0x5808+j)^1]=header[8+j];
    sr=bram(3,0x5808,0x60000,&size); CHECK(sr==0x2010 && size==30);
    sr=bram(8,0x5808,0x60000,&size); CHECK(sr==0x2010);
    Ram_Prg[0x60000^1]^=1;
    sr=bram(8,0x5808,0x60000,&size); CHECK(sr==0x2011);
    // Invalid size/destination must fail without damaging the saved record.
    word(0x5814,126);
    sr=bram(4,0x5808,0x60000,&size); CHECK(sr==0x2011);
    sr=bram(3,0x5808,0x7ffff,&size); CHECK(sr==0x2011);
    sr=bram(3,0x5808,0x60000,&size); CHECK(sr==0x2010 && size==30);
    Reset_SegaCD(); CHECK(CD_HLE_Active && !Paused);
    CHECK(main68k_context.pc==0xff0000 && sub68k_context.pc==0x1000);
    Free_Rom(Game); CHECK(!CD_HLE_Active && !SegaCD_Started);
    search=FindFirstFileA("*.brm",&found); CHECK(search==INVALID_HANDLE_VALUE);
    FILE_End();
    printf("PASS: %s, %s timing, 9000 frames, %u CD-audio frames; save API, reset and teardown.\n",
           resume?"continue":"new game and save",accurate?"accurate":"normal",audio_frames);
    return 0;
}
