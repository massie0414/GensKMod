#include <string.h>
#include "program_ram_transfer.h"

static unsigned long history[2][PROGRAM_RAM_TRANSFER_HISTORY];
static unsigned int next_sample, sample_count, frame_rate;

/* Shared only with the register-preserving S68K_Data_* assembly thunks in
 * word_ram_transfer.c. Avoid calling C from Starscream's memory helpers. */
unsigned long ProgramRAMTransfer_ReadBytes, ProgramRAMTransfer_WriteBytes;
int ProgramRAMTransfer_InFrame;

void ProgramRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    ProgramRAMTransfer_ReadBytes = ProgramRAMTransfer_WriteBytes = 0;
    next_sample = sample_count = frame_rate = 0;
    ProgramRAMTransfer_InFrame = 0;
}

void ProgramRAMTransfer_BeginFrame(void)
{
    ProgramRAMTransfer_ReadBytes = ProgramRAMTransfer_WriteBytes = 0;
    ProgramRAMTransfer_InFrame = 1;
}

void ProgramRAMTransfer_AddBytes(int write, unsigned long bytes)
{
    if (!ProgramRAMTransfer_InFrame) return;
    if (write) ProgramRAMTransfer_WriteBytes += bytes;
    else ProgramRAMTransfer_ReadBytes += bytes;
}

void ProgramRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long reads = ProgramRAMTransfer_ReadBytes, writes = ProgramRAMTransfer_WriteBytes;
    if (!ProgramRAMTransfer_InFrame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) ProgramRAMTransfer_Reset();
    frame_rate = frames_per_second;
    history[PROGRAM_RAM_TRANSFER_READ][next_sample] = reads;
    history[PROGRAM_RAM_TRANSFER_WRITE][next_sample] = writes;
    next_sample = (next_sample + 1) % PROGRAM_RAM_TRANSFER_HISTORY;
    if (sample_count < PROGRAM_RAM_TRANSFER_HISTORY) ++sample_count;
    ProgramRAMTransfer_ReadBytes = ProgramRAMTransfer_WriteBytes = 0;
    ProgramRAMTransfer_InFrame = 0;
}

unsigned int ProgramRAMTransfer_GetHistory(int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[write != 0][(next_sample + PROGRAM_RAM_TRANSFER_HISTORY - sample_count + i) % PROGRAM_RAM_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long ProgramRAMTransfer_GetBytesPerSecond(int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[write != 0][(next_sample + PROGRAM_RAM_TRANSFER_HISTORY - 1 - i) % PROGRAM_RAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}
