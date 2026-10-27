#include <string.h>
#include "vram_transfer.h"
#include "Mem_M68K.h"
#include "vdp_io.h"

extern int CD_32X_Active;

static unsigned long history[VDP_TRANSFER_KINDS][2][VRAM_TRANSFER_HISTORY];
static unsigned long pending[VDP_TRANSFER_KINDS][2];
static unsigned int next_sample, sample_count, frame_rate;
static int in_frame;

void VRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    memset(pending, 0, sizeof(pending));
    next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void VRAMTransfer_BeginFrame(void)
{
    memset(pending, 0, sizeof(pending));
    in_frame = Genesis_Started || SegaCD_Started || _32X_Started || CD_32X_Active;
}

void VRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned int kind;
    unsigned long completed[VDP_TRANSFER_KINDS][2];
    memcpy(completed, pending, sizeof(completed));
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) VRAMTransfer_Reset();
    frame_rate = frames_per_second;
    for (kind = 0; kind < VDP_TRANSFER_KINDS; ++kind) {
        history[kind][VRAM_TRANSFER_READ][next_sample] = completed[kind][0];
        history[kind][VRAM_TRANSFER_WRITE][next_sample] = completed[kind][1];
    }
    next_sample = (next_sample + 1) % VRAM_TRANSFER_HISTORY;
    if (sample_count < VRAM_TRANSFER_HISTORY) ++sample_count;
    memset(pending, 0, sizeof(pending));
    in_frame = 0;
}

unsigned int VDPTransfer_GetHistory(int kind, int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[kind][write != 0][(next_sample + VRAM_TRANSFER_HISTORY - sample_count + i) % VRAM_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long VDPTransfer_GetBytesPerSecond(int kind, int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[kind][write != 0][(next_sample + VRAM_TRANSFER_HISTORY - 1 - i) % VRAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}

unsigned int VRAMTransfer_GetHistory(int write, unsigned long *values)
{
    return VDPTransfer_GetHistory(VDP_TRANSFER_VRAM, write, values);
}

unsigned long VRAMTransfer_GetBytesPerSecond(int write)
{
    return VDPTransfer_GetBytesPerSecond(VDP_TRANSFER_VRAM, write);
}

/* Called only in actual DMA transfer paths, after validation. Fill has no
 * source-memory reads; writes include the initial seed byte stored in VRAM. */
void VDPTransfer_RecordDMA(unsigned long reads, unsigned long writes)
{
    if (!in_frame) return;
    pending[VDP_TRANSFER_DMA][0] += reads;
    pending[VDP_TRANSFER_DMA][1] += writes;
}

void VDPTransfer_RecordHLEWrite(int kind, unsigned long bytes)
{
    if (in_frame) pending[kind][1] += bytes;
}

/* Match the VDP's access selector: 5/6/7 read VRAM/CRAM/VSRAM;
 * 9/10/11 write them. Preserve ECX for Starscream's longword helpers.
 * Unsupported byte reads never reach the VDP in this emulator. */
#define MEASURE_PORT(direction, bytes, access_base, dma_mask, raw) \
    __asm push ecx \
    __asm cmp in_frame, 0 \
    __asm je forward_access \
    __asm mov ecx, [esp + 8] \
    __asm and ecx, 0xFFFFFF \
    __asm cmp ecx, 0xC00000 \
    __asm jb forward_access \
    __asm cmp ecx, 0xC00003 \
    __asm ja forward_access \
    __asm mov ecx, Ctrl.Access \
    __asm sub ecx, access_base \
    __asm cmp ecx, 2 \
    __asm ja forward_access \
    __asm test Ctrl.DMA, dma_mask \
    __asm jnz forward_access \
    __asm add dword ptr pending[ecx * 8 + direction * 4], bytes \
    __asm forward_access: \
    __asm pop ecx \
    __asm jmp raw

__declspec(naked) unsigned short M68K_VRAM_RW(unsigned int address)
{
    MEASURE_PORT(0, 2, 5, 0, M68K_RW)
}
__declspec(naked) void M68K_VRAM_WB(unsigned int address, unsigned char value)
{
    MEASURE_PORT(1, 1, 9, 4, M68K_WB)
}
__declspec(naked) void M68K_VRAM_WW(unsigned int address, unsigned short value)
{
    MEASURE_PORT(1, 2, 9, 4, M68K_WW)
}
#undef MEASURE_PORT
