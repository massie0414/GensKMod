#ifndef PROGRAM_RAM_TRANSFER_H
#define PROGRAM_RAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif

#define PROGRAM_RAM_TRANSFER_HISTORY 256
#define PROGRAM_RAM_TRANSFER_READ 0
#define PROGRAM_RAM_TRANSFER_WRITE 1

/* Sub-68000 data bus bytes in $000000-$07FFFF, including stack accesses
 * and writes rejected by write protection. Instruction fetch, main CPU,
 * DMA, boot initialization and debugger inspection are excluded. */
void ProgramRAMTransfer_Reset(void);
void ProgramRAMTransfer_BeginFrame(void);
void ProgramRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int ProgramRAMTransfer_GetHistory(int write, unsigned long *values);
unsigned long ProgramRAMTransfer_GetBytesPerSecond(int write);

/* Account for sub-CPU BIOS HLE accesses that operate directly on Ram_Prg.
 * Native CPU accesses are measured by the S68K_Data_* thunks. */
void ProgramRAMTransfer_AddBytes(int write, unsigned long bytes);

#ifdef __cplusplus
}
#endif
#endif
