#include <string.h>
#include "vram32x_transfer.h"
#include "vdp_io.h"
extern int CD_32X_Active;

static unsigned long history[VRAM32X_TRANSFER_HISTORY];
/* Updated at native stores, after transparent/access checks. */
unsigned long VRAM32XTransfer_Pending;
#define pending VRAM32XTransfer_Pending
static unsigned int next_sample, sample_count, frame_rate;
int VRAM32XTransfer_InFrame;
#define in_frame VRAM32XTransfer_InFrame

void VRAM32XTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    pending = next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void VRAM32XTransfer_BeginFrame(void)
{
    pending = 0;
    in_frame = _32X_Started || CD_32X_Active;
}

void VRAM32XTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long bytes = pending;
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) VRAM32XTransfer_Reset();
    frame_rate = frames_per_second;
    history[next_sample] = bytes;
    next_sample = (next_sample + 1) % VRAM32X_TRANSFER_HISTORY;
    if (sample_count < VRAM32X_TRANSFER_HISTORY) ++sample_count;
    pending = 0;
    in_frame = 0;
}

unsigned int VRAM32XTransfer_GetHistory(unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[(next_sample + VRAM32X_TRANSFER_HISTORY - sample_count + i) % VRAM32X_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long VRAM32XTransfer_GetBytesPerSecond(void)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[(next_sample + VRAM32X_TRANSFER_HISTORY - 1 - i) % VRAM32X_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}
