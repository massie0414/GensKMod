#include <string.h>
#include "sdram_transfer.h"
#include "vdp_io.h"
extern int CD_32X_Active;

static unsigned long history[2][SDRAM_TRANSFER_HISTORY];
/* Native RAM handlers update these without changing SH2_CONTEXT/save states. */
unsigned long SDRAMTransfer_Pending[2];
int SDRAMTransfer_InFrame;
/* Balanced around DMA pairs and exception entry, including nested requests. */
unsigned int SDRAMTransfer_BusActive;
static unsigned int next_sample, sample_count, frame_rate;

void SDRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    memset(SDRAMTransfer_Pending, 0, sizeof(SDRAMTransfer_Pending));
    next_sample = sample_count = frame_rate = 0;
    SDRAMTransfer_InFrame = 0;
}
void SDRAMTransfer_BeginFrame(void)
{
    SDRAMTransfer_Pending[0] = SDRAMTransfer_Pending[1] = 0;
    SDRAMTransfer_InFrame = _32X_Started || CD_32X_Active;
}
void SDRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long reads = SDRAMTransfer_Pending[0], writes = SDRAMTransfer_Pending[1];
    if (!SDRAMTransfer_InFrame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) SDRAMTransfer_Reset();
    frame_rate = frames_per_second;
    history[0][next_sample] = reads;
    history[1][next_sample] = writes;
    next_sample = (next_sample + 1) % SDRAM_TRANSFER_HISTORY;
    if (sample_count < SDRAM_TRANSFER_HISTORY) ++sample_count;
    SDRAMTransfer_Pending[0] = SDRAMTransfer_Pending[1] = 0;
    SDRAMTransfer_InFrame = 0;
}
unsigned int SDRAMTransfer_GetHistory(int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[write != 0][(next_sample + SDRAM_TRANSFER_HISTORY - sample_count + i) % SDRAM_TRANSFER_HISTORY];
    return sample_count;
}
unsigned long SDRAMTransfer_GetBytesPerSecond(int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[write != 0][(next_sample + SDRAM_TRANSFER_HISTORY - 1 - i) % SDRAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}
