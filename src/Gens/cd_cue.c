#include "cd_cue.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

int CD_Cue_Read(const char *path, CD_Cue *cue)
{
    FILE *f, *bin;
    char line[2048], name[1024], kind[32];
    int current=-1, files=0, gap=0, gap_set=0, inserted=0, index0=-1, i;
    long size;
    const char *slash, *other;
    if(!cue)return 0;
    memset(cue,0,sizeof(*cue));
    if(!path || strlen(path)>=1024)return 0;
    f=fopen(path,"r"); if(!f)return 0;
    while(fgets(line,sizeof(line),f)) {
        int n, index, m,s,fr,pos;
        char *p=line;
        if(!strchr(line,'\n') && !feof(f))goto bad;
        while(*p==' '||*p=='\t')p++;
        if(!strncmp(p,"FILE ",5)) {
            if(++files!=1 || sscanf(p,"FILE \"%1023[^\"]\" %31s",name,kind)!=2 || strcmp(kind,"BINARY"))goto bad;
            slash=strrchr(path,'\\'); other=strrchr(path,'/'); if(other && (!slash||other>slash))slash=other;
            n=slash?(int)(slash-path+1):0;
            if(name[0]=='/'||name[0]=='\\'||strchr(name,':'))n=0;
            if(n+strlen(name)>=sizeof(cue->file))goto bad;
            memcpy(cue->file,path,n); strcpy(cue->file+n,name);
        } else if(!strncmp(p,"TRACK ",6)) {
            if(files!=1 || sscanf(p,"TRACK %d %31s",&n,kind)!=2 || n!=cue->count+1 || n>99)goto bad;
            if(strcmp(kind,n==1?"MODE1/2352":"AUDIO"))goto bad;
            if(current>=0 && cue->track[current].offset<0)goto bad;
            current=cue->count++; cue->track[current].offset=-1; gap=gap_set=0; index0=-1;
        } else if(!strncmp(p,"PREGAP ",7)) {
            if(current<=0 || gap_set || cue->track[current].offset>=0 || sscanf(p,"PREGAP %d:%d:%d",&m,&s,&fr)!=3 || m<0||m>99||s<0||s>59||fr<0||fr>74)goto bad;
            gap=(m*60+s)*75+fr; gap_set=1;
        } else if(!strncmp(p,"INDEX ",6)) {
            if(current<0 || sscanf(p,"INDEX %d %d:%d:%d",&index,&m,&s,&fr)!=4 || m<0||m>99||s<0||s>59||fr<0||fr>74)goto bad;
            pos=(m*60+s)*75+fr;
            if(index==0) { if(index0>=0 || cue->track[current].offset>=0)goto bad; index0=pos; }
            else if(index==1) {
                if(cue->track[current].offset>=0 || (index0>=0 && index0>pos))goto bad;
                if(current==0 && pos!=0)goto bad;
                if(current>0) {
                    int end=index0>=0?index0:pos;
                    if(end<=cue->track[current-1].offset)goto bad;
                    cue->track[current-1].length=end-cue->track[current-1].offset;
                }
                inserted+=gap;
                cue->track[current].offset=pos;
                cue->track[current].start=pos+inserted;
            } else goto bad;
        } else if(!strncmp(p,"POSTGAP ",8))goto bad;
    }
    if(ferror(f))goto bad;
    fclose(f);
    if(cue->count==0 || cue->track[cue->count-1].offset<0)return 0;
    bin=fopen(cue->file,"rb"); if(!bin)return 0;
    if(fseek(bin,0,SEEK_END)) { fclose(bin); return 0; }
    size=ftell(bin); fclose(bin);
    if(size<=0 || size%2352 || size/2352>INT_MAX-inserted)return 0;
    cue->sectors=(int)(size/2352); cue->leadout=cue->sectors+inserted;
    for(i=0;i<cue->count;i++)if(cue->track[i].offset>=cue->sectors)return 0;
    cue->track[cue->count-1].length=cue->sectors-cue->track[cue->count-1].offset;
    return 1;
bad:
    fclose(f); memset(cue,0,sizeof(*cue)); return 0;
}
