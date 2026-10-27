#include <string.h>
#include "pwm_fifo_transfer.h"
#include "vdp_io.h"
extern int CD_32X_Active;

static unsigned long history[2][PWM_FIFO_TRANSFER_HISTORY];
/* Native FIFO enqueue/dequeue sites count accepted 16-bit sample payload. */
unsigned long PWMFIFOTransfer_Pending[2];
int PWMFIFOTransfer_InFrame;
static unsigned int next_sample, sample_count, frame_rate;

void PWMFIFOTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    memset(PWMFIFOTransfer_Pending, 0, sizeof(PWMFIFOTransfer_Pending));
    next_sample = sample_count = frame_rate = 0;
    PWMFIFOTransfer_InFrame = 0;
}
void PWMFIFOTransfer_BeginFrame(void)
{
    PWMFIFOTransfer_Pending[0] = PWMFIFOTransfer_Pending[1] = 0;
    PWMFIFOTransfer_InFrame = _32X_Started || CD_32X_Active;
}
void PWMFIFOTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long reads = PWMFIFOTransfer_Pending[0], writes = PWMFIFOTransfer_Pending[1];
    if (!PWMFIFOTransfer_InFrame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) PWMFIFOTransfer_Reset();
    frame_rate = frames_per_second;
    history[0][next_sample] = reads;
    history[1][next_sample] = writes;
    next_sample = (next_sample + 1) % PWM_FIFO_TRANSFER_HISTORY;
    if (sample_count < PWM_FIFO_TRANSFER_HISTORY) ++sample_count;
    PWMFIFOTransfer_Pending[0] = PWMFIFOTransfer_Pending[1] = 0;
    PWMFIFOTransfer_InFrame = 0;
}
unsigned int PWMFIFOTransfer_GetHistory(int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[write != 0][(next_sample + PWM_FIFO_TRANSFER_HISTORY - sample_count + i) % PWM_FIFO_TRANSFER_HISTORY];
    return sample_count;
}
unsigned long PWMFIFOTransfer_GetBytesPerSecond(int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[write != 0][(next_sample + PWM_FIFO_TRANSFER_HISTORY - 1 - i) % PWM_FIFO_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}
