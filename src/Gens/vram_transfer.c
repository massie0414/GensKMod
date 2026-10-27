#include <string.h>
#include "vram_transfer.h"
#include "Mem_M68K.h"
#include "vdp_io.h"

extern int CD_32X_Active;

static unsigned long history[2][VRAM_TRANSFER_HISTORY];
static unsigned long pending_read, pending_write;
static unsigned int next_sample, sample_count, frame_rate;
static int in_frame;

void VRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    pending_read = pending_write = next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void VRAMTransfer_BeginFrame(void)
{
    pending_read = pending_write = 0;
    in_frame = Genesis_Started || SegaCD_Started || _32X_Started || CD_32X_Active;
}

void VRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long reads = pending_read, writes = pending_write;
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) VRAMTransfer_Reset();
    frame_rate = frames_per_second;
    history[VRAM_TRANSFER_READ][next_sample] = reads;
    history[VRAM_TRANSFER_WRITE][next_sample] = writes;
    next_sample = (next_sample + 1) % VRAM_TRANSFER_HISTORY;
    if (sample_count < VRAM_TRANSFER_HISTORY) ++sample_count;
    pending_read = pending_write = 0;
    in_frame = 0;
}

unsigned int VRAMTransfer_GetHistory(int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[write != 0][(next_sample + VRAM_TRANSFER_HISTORY - sample_count + i) % VRAM_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long VRAMTransfer_GetBytesPerSecond(int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[write != 0][(next_sample + VRAM_TRANSFER_HISTORY - 1 - i) % VRAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}

/* Match Mem_M68k.asm and vdp_io.asm, including exact data-port decoding
 * and the VDP access selector before the transfer changes its state.
 * Byte reads do not reach Read_VDP_Data in this emulator, so only word
 * reads are measured. Preserve ECX for Starscream's longword helpers. */
#define MEASURE_PORT(counter, bytes, access, dma_mask, raw) \
    __asm push ecx \
    __asm cmp in_frame, 0 \
    __asm je forward_access \
    __asm mov ecx, [esp + 8] \
    __asm and ecx, 0xFFFFFF \
    __asm cmp ecx, 0xC00000 \
    __asm jb forward_access \
    __asm cmp ecx, 0xC00003 \
    __asm ja forward_access \
    __asm cmp Ctrl.Access, access \
    __asm jne forward_access \
    __asm test Ctrl.DMA, dma_mask \
    __asm jnz forward_access \
    __asm add counter, bytes \
    __asm forward_access: \
    __asm pop ecx \
    __asm jmp raw

__declspec(naked) unsigned short M68K_VRAM_RW(unsigned int address)
{
    MEASURE_PORT(pending_read, 2, 5, 0, M68K_RW)
}
__declspec(naked) void M68K_VRAM_WB(unsigned int address, unsigned char value)
{
    MEASURE_PORT(pending_write, 1, 9, 4, M68K_WB)
}
__declspec(naked) void M68K_VRAM_WW(unsigned int address, unsigned short value)
{
    MEASURE_PORT(pending_write, 2, 9, 4, M68K_WW)
}
#undef MEASURE_PORT
