#include <string.h>
#include "main_word_ram_transfer.h"
#include "Mem_M68K.h"

extern int SegaCD_Started, CD_32X_Active;

static unsigned long history[2][MAIN_WORD_RAM_TRANSFER_HISTORY];
static unsigned long pending_read, pending_write;
static unsigned int next_sample, sample_count, frame_rate;
static int in_frame;

void MainWordRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    pending_read = pending_write = next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void MainWordRAMTransfer_BeginFrame(void)
{
    pending_read = pending_write = 0;
    in_frame = SegaCD_Started || CD_32X_Active;
}

void MainWordRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned long reads = pending_read, writes = pending_write;
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) MainWordRAMTransfer_Reset();
    frame_rate = frames_per_second;
    history[MAIN_WORD_RAM_TRANSFER_READ][next_sample] = reads;
    history[MAIN_WORD_RAM_TRANSFER_WRITE][next_sample] = writes;
    next_sample = (next_sample + 1) % MAIN_WORD_RAM_TRANSFER_HISTORY;
    if (sample_count < MAIN_WORD_RAM_TRANSFER_HISTORY) ++sample_count;
    pending_read = pending_write = 0;
    in_frame = 0;
}

unsigned int MainWordRAMTransfer_GetHistory(int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[write != 0][(next_sample + MAIN_WORD_RAM_TRANSFER_HISTORY - sample_count + i) % MAIN_WORD_RAM_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long MainWordRAMTransfer_GetBytesPerSecond(int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[write != 0][(next_sample + MAIN_WORD_RAM_TRANSFER_HISTORY - 1 - i) % MAIN_WORD_RAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}

/* Starscream keeps part of a longword in ECX across raw memory calls.
 * Preserve registers and arguments, then tail-call the existing handler.
 * Both 2M and 1M (linear/cell-arranged) use $200000-$23FFFF. Match the
 * emulator's permissive 2M ownership behavior without changing it. */
#define MEASURE_ACCESS(counter, bytes, raw) \
    __asm push ecx \
    __asm cmp in_frame, 0 \
    __asm je forward_access \
    __asm mov ecx, [esp + 8] \
    __asm and ecx, 0xFFFFFF \
    __asm cmp ecx, 0x200000 \
    __asm jb forward_access \
    __asm cmp ecx, 0x23FFFF \
    __asm ja forward_access \
    __asm add counter, bytes \
    __asm forward_access: \
    __asm pop ecx \
    __asm jmp raw

__declspec(naked) unsigned char M68K_Data_RB(unsigned int address)
{
    MEASURE_ACCESS(pending_read, 1, M68K_RB)
}
__declspec(naked) unsigned short M68K_Data_RW(unsigned int address)
{
    MEASURE_ACCESS(pending_read, 2, M68K_RW)
}
__declspec(naked) void M68K_Data_WB(unsigned int address, unsigned char value)
{
    MEASURE_ACCESS(pending_write, 1, M68K_WB)
}
__declspec(naked) void M68K_Data_WW(unsigned int address, unsigned short value)
{
    MEASURE_ACCESS(pending_write, 2, M68K_WW)
}
#undef MEASURE_ACCESS
