#include <string.h>
#include "genesis_ram_transfer.h"
#include "vram_transfer.h"
#include "Mem_M68K.h"
#include "vdp_io.h"

extern int CD_32X_Active;

static unsigned long history[GENESIS_RAM_TRANSFER_KINDS][2][GENESIS_RAM_TRANSFER_HISTORY];
/* Shared with the native core fast path; offsets 0/4 are main-RAM read/write. */
unsigned long GenesisRAMTransfer_Pending[GENESIS_RAM_TRANSFER_KINDS][2];
#define pending GenesisRAMTransfer_Pending
static unsigned int next_sample, sample_count, frame_rate;
int GenesisRAMTransfer_InFrame;
#define in_frame GenesisRAMTransfer_InFrame

void GenesisRAMTransfer_Reset(void)
{
    memset(history, 0, sizeof(history));
    memset(pending, 0, sizeof(pending));
    next_sample = sample_count = frame_rate = 0;
    in_frame = 0;
}

void GenesisRAMTransfer_BeginFrame(void)
{
    memset(pending, 0, sizeof(pending));
    in_frame = Genesis_Started || SegaCD_Started || _32X_Started || CD_32X_Active;
}

void GenesisRAMTransfer_EndFrame(unsigned int frames_per_second)
{
    unsigned int kind;
    unsigned long completed[GENESIS_RAM_TRANSFER_KINDS][2];
    memcpy(completed, pending, sizeof(completed));
    if (!in_frame || !frames_per_second) return;
    if (frame_rate && frame_rate != frames_per_second) GenesisRAMTransfer_Reset();
    frame_rate = frames_per_second;
    for (kind = 0; kind < GENESIS_RAM_TRANSFER_KINDS; ++kind) {
        history[kind][GENESIS_RAM_TRANSFER_READ][next_sample] = completed[kind][0];
        history[kind][GENESIS_RAM_TRANSFER_WRITE][next_sample] = completed[kind][1];
    }
    next_sample = (next_sample + 1) % GENESIS_RAM_TRANSFER_HISTORY;
    if (sample_count < GENESIS_RAM_TRANSFER_HISTORY) ++sample_count;
    memset(pending, 0, sizeof(pending));
    in_frame = 0;
}

unsigned int GenesisRAMTransfer_GetHistory(int kind, int write, unsigned long *values)
{
    unsigned int i;
    for (i = 0; i < sample_count; ++i)
        values[i] = history[kind][write != 0][(next_sample + GENESIS_RAM_TRANSFER_HISTORY - sample_count + i) % GENESIS_RAM_TRANSFER_HISTORY];
    return sample_count;
}

unsigned long GenesisRAMTransfer_GetBytesPerSecond(int kind, int write)
{
    unsigned int i, count = sample_count < frame_rate ? sample_count : frame_rate;
    unsigned __int64 total = 0;
    if (!count) return 0;
    for (i = 0; i < count; ++i)
        total += history[kind][write != 0][(next_sample + GENESIS_RAM_TRANSFER_HISTORY - 1 - i) % GENESIS_RAM_TRANSFER_HISTORY];
    return (unsigned long)(total * frame_rate / count);
}

/* HLE operations that retain direct backing-store semantics. */
void GenesisRAMTransfer_AddMainBytes(int write, unsigned long bytes)
{
    if (in_frame) pending[GENESIS_RAM_MAIN][write != 0] += bytes;
}

/* Main RAM mirrors throughout $E00000-$FFFFFF. Z80 RAM is mirrored in
 * $A00000-$A03FFF and $A08000-$A0BFFF by the current memory handlers.
 * Those handlers transfer one Z80 byte even for a 68000 word access.
 * Preserve ECX for Starscream's split longword operations. */
#define MEASURE_RAM(direction, main_bytes, raw) \
    __asm push ecx \
    __asm cmp in_frame, 0 \
    __asm je forward_access \
    __asm mov ecx, [esp + 8] \
    __asm and ecx, 0xFFFFFF \
    __asm cmp ecx, 0xE00000 \
    __asm jae main_ram \
    __asm and ecx, 0xFF4000 \
    __asm cmp ecx, 0xA00000 \
    __asm jne forward_access \
    __asm test byte ptr Z80_State, 6 \
    __asm jnz forward_access \
    __asm add dword ptr pending[8 + direction * 4], 1 \
    __asm jmp forward_access \
    __asm main_ram: \
    __asm add dword ptr pending[direction * 4], main_bytes \
    __asm forward_access: \
    __asm pop ecx \
    __asm jmp raw

__declspec(naked) unsigned char M68K_RAM_RB(unsigned int address)
{
    MEASURE_RAM(0, 1, M68K_RB)
}
__declspec(naked) unsigned short M68K_RAM_RW(unsigned int address)
{
    MEASURE_RAM(0, 2, M68K_VRAM_RW)
}
__declspec(naked) void M68K_RAM_WB(unsigned int address, unsigned char value)
{
    MEASURE_RAM(1, 1, M68K_VRAM_WB)
}
__declspec(naked) void M68K_RAM_WW(unsigned int address, unsigned short value)
{
    MEASURE_RAM(1, 2, M68K_VRAM_WW)
}
#undef MEASURE_RAM
