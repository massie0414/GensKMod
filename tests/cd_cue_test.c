#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/Gens/cd_cue.h"

static void fixture(const char *text)
{
    FILE *f = fopen("fixture.cue", "w");
    assert(f);
    assert(fputs(text, f) >= 0);
    assert(fclose(f) == 0);
}

int main(void)
{
    CD_Cue cue;
    FILE *f = fopen("fixture.bin", "wb");
    assert(f);
    assert(fseek(f, 1000 * 2352 - 1, SEEK_SET) == 0);
    assert(fputc(0, f) != EOF);
    assert(fclose(f) == 0);

    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n"
            " TRACK 02 AUDIO\n PREGAP 00:02:00\n INDEX 01 00:04:00\n"
            " TRACK 03 AUDIO\n INDEX 00 00:07:00\n INDEX 01 00:08:00\n");
    assert(CD_Cue_Read("fixture.cue", &cue));
    assert(cue.count == 3 && cue.sectors == 1000 && cue.leadout == 1150);
    assert(cue.track[0].length == 300);
    assert(cue.track[1].offset == 300 && cue.track[1].start == 450);
    assert(cue.track[1].length == 225);
    assert(cue.track[2].offset == 600 && cue.track[2].start == 750);
    assert(cue.track[2].length == 400);

    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n"
            " FILE \"fixture.bin\" BINARY\n TRACK 02 AUDIO\n INDEX 01 00:00:00\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2048\n INDEX 01 00:00:00\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n"
            " TRACK 02 AUDIO\n INDEX 01 00:20:00\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:75\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n"
            " TRACK 02 AUDIO\n PREGAP 00:02:00\n PREGAP 00:02:00\n INDEX 01 00:04:00\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n"
            " TRACK 02 AUDIO\n INDEX 01 00:04:00\n PREGAP 00:02:00\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    f = fopen("fixture.bin", "ab");
    assert(f); fputc(0, f); fclose(f);
    fixture("FILE \"fixture.bin\" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n");
    assert(!CD_Cue_Read("fixture.cue", &cue));
    remove("fixture.cue"); remove("fixture.bin");
    puts("CUE parser tests passed.");
    return 0;
}
