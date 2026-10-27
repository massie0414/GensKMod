#include <string.h>
#include "cd_transfer.h"

static unsigned long history[CD_TRANSFER_HISTORY], pending;
static unsigned int next_sample, sample_count, frame_rate;
static int in_frame;

void CDTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    pending = next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void CDTransfer_BeginFrame(void)
{
    pending = 0;
    in_frame = 1;
}

void CDTransfer_AddBytes(unsigned long bytes)
{
    /* Boot probes and debugger reads outside a running frame are excluded. */
    if (in_frame) pending += bytes;
}

void CDTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long bytes = pending;
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) CDTransfer_Reset();
    frame_rate = frames_per_second;
    history[next_sample] = bytes;
    next_sample = (next_sample + 1) % CD_TRANSFER_HISTORY;
    if (sample_count < CD_TRANSFER_HISTORY) ++sample_count;
    pending = 0;
    in_frame = 0;
}

unsigned int CDTransfer_GetHistory(unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[(next_sample + CD_TRANSFER_HISTORY - sample_count + i) % CD_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long CDTransfer_GetBytesPerSecond(void)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[(next_sample + CD_TRANSFER_HISTORY - 1 - i) % CD_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}
