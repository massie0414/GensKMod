#include <string.h>
#include "sh2_dma_transfer.h"
#include "vdp_io.h"
extern int CD_32X_Active;

static struct TransferState {
    unsigned long history[2][SH2_DMA_TRANSFER_HISTORY], pending[2];
    unsigned int next, count, frame_rate;
} state[SH2_DMA_TRANSFER_KINDS];
static int in_frame;

static int CPUIndex(SH2_CONTEXT *cpu)
{
    return cpu == &M_SH2 ? SH2_DMA_MASTER : cpu == &S_SH2 ? SH2_DMA_SLAVE : -1;
}
void SH2DMATransfer_Reset(void)
{
    memset(state, 0, sizeof(state));
    in_frame = 0;
}
void SH2DMATransfer_ClearCPU(SH2_CONTEXT *cpu)
{
    int index = CPUIndex(cpu);
    if (index >= 0) memset(&state[index], 0, sizeof(state[index]));
}
void SH2DMATransfer_BeginFrame(void)
{
    int cpu;
    for (cpu = 0; cpu < SH2_DMA_TRANSFER_KINDS; ++cpu)
        state[cpu].pending[0] = state[cpu].pending[1] = 0;
    in_frame = _32X_Started || CD_32X_Active;
}
void SH2DMATransfer_EndFrame(unsigned int frames_per_second)
{
    int cpu;
    if (!in_frame || !frames_per_second) return;
    for (cpu = 0; cpu < SH2_DMA_TRANSFER_KINDS; ++cpu) {
        struct TransferState *s = &state[cpu];
        unsigned long reads = s->pending[0], writes = s->pending[1];
        if (s->frame_rate && s->frame_rate != frames_per_second) memset(s, 0, sizeof(*s));
        s->frame_rate = frames_per_second;
        s->history[0][s->next] = reads;
        s->history[1][s->next] = writes;
        s->next = (s->next + 1) % SH2_DMA_TRANSFER_HISTORY;
        if (s->count < SH2_DMA_TRANSFER_HISTORY) ++s->count;
        s->pending[0] = s->pending[1] = 0;
    }
    in_frame = 0;
}
unsigned int SH2DMATransfer_GetHistory(int cpu, int write, unsigned long *values)
{
    struct TransferState *s = &state[cpu];
    unsigned int i;
    for (i = 0; i < s->count; ++i)
        values[i] = s->history[write != 0][(s->next + SH2_DMA_TRANSFER_HISTORY - s->count + i) % SH2_DMA_TRANSFER_HISTORY];
    return s->count;
}
unsigned long SH2DMATransfer_GetBytesPerSecond(int cpu, int write)
{
    struct TransferState *s = &state[cpu];
    unsigned int i, count = s->count < s->frame_rate ? s->count : s->frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += s->history[write != 0][(s->next + SH2_DMA_TRANSFER_HISTORY - 1 - i) % SH2_DMA_TRANSFER_HISTORY];
    return (unsigned long)(total * s->frame_rate / count);
}
void SH2DMATransfer_AddBytes(SH2_CONTEXT *cpu, unsigned long bytes)
{
    int index;
    if (!in_frame) return;
    index = CPUIndex(cpu);
    if (index < 0) return;
    state[index].pending[0] += bytes;
    state[index].pending[1] += bytes;
}
