#ifndef CD_CUE_H
#define CD_CUE_H
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Single-file, raw MODE1/2352 + AUDIO images. Sector positions exclude lead-in. */
typedef struct {
    char file[1024];
    int count, sectors, leadout;
    struct { int offset, start, length; } track[99];
} CD_Cue;
int CD_Cue_Read(const char *path, CD_Cue *cue);
#ifdef __cplusplus
}
#endif
#endif
