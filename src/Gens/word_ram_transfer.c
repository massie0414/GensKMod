#include <string.h>
#include "word_ram_transfer.h"
#include "Mem_S68K.h"

static unsigned long history[2][WORD_RAM_TRANSFER_HISTORY];
static unsigned long pending_read, pending_write;
static unsigned int next_sample, sample_count, frame_rate;
static int in_frame;

void WordRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    pending_read = pending_write = next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void WordRAMTransfer_BeginFrame(void)
{
    pending_read = pending_write = 0;
    in_frame = 1;
}

void WordRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long reads = pending_read, writes = pending_write;
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) WordRAMTransfer_Reset();
    frame_rate = frames_per_second;
    history[WORD_RAM_TRANSFER_READ][next_sample] = reads;
    history[WORD_RAM_TRANSFER_WRITE][next_sample] = writes;
    next_sample = (next_sample + 1) % WORD_RAM_TRANSFER_HISTORY;
    if (sample_count < WORD_RAM_TRANSFER_HISTORY) ++sample_count;
    pending_read = pending_write = 0;
    in_frame = 0;
}

unsigned int WordRAMTransfer_GetHistory(int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[write != 0][(next_sample + WORD_RAM_TRANSFER_HISTORY - sample_count + i) % WORD_RAM_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long WordRAMTransfer_GetBytesPerSecond(int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[write != 0][(next_sample + WORD_RAM_TRANSFER_HISTORY - 1 - i) % WORD_RAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}

/* Starscream's longword helpers keep half the value in ECX between two
 * accesses. Ordinary C wrappers may clobber it. These x86 tail-call thunks
 * preserve every register and the original argument stack before entering
 * the existing memory handlers; emulated timing and memory behavior stay
 * unchanged. Match Mem_S68k.asm: 2M $080000-$0BFFFF; 1M also $0C0000-$0DFFFF.
 * Count bus width, including repeat/same-value writes, not changed bytes. */
#define MEASURE_ACCESS(counter, bytes, raw) \
    __asm push ecx \
    __asm mov ecx, [esp + 8] \
    __asm cmp in_frame, 0 \
    __asm je forward_access \
    __asm cmp ecx, 0x080000 \
    __asm jb forward_access \
    __asm test Ram_Word_State, 2 \
    __asm jnz banked_access \
    __asm cmp ecx, 0x0BFFFF \
    __asm jmp check_access \
    __asm banked_access: \
    __asm cmp ecx, 0x0DFFFF \
    __asm check_access: \
    __asm ja forward_access \
    __asm add counter, bytes \
    __asm forward_access: \
    __asm pop ecx \
    __asm jmp raw

__declspec(naked) unsigned char S68K_Data_RB(unsigned int address)
{
    MEASURE_ACCESS(pending_read, 1, S68K_RB)
}
__declspec(naked) unsigned short S68K_Data_RW(unsigned int address)
{
    MEASURE_ACCESS(pending_read, 2, S68K_RW)
}
__declspec(naked) void S68K_Data_WB(unsigned int address, unsigned char value)
{
    MEASURE_ACCESS(pending_write, 1, S68K_WB)
}
__declspec(naked) void S68K_Data_WW(unsigned int address, unsigned short value)
{
    MEASURE_ACCESS(pending_write, 2, S68K_WW)
}
#undef MEASURE_ACCESS
