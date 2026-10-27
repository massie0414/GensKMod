#ifndef MAIN_WORD_RAM_TRANSFER_H
#define MAIN_WORD_RAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif

#define MAIN_WORD_RAM_TRANSFER_HISTORY 256
#define MAIN_WORD_RAM_TRANSFER_READ 0
#define MAIN_WORD_RAM_TRANSFER_WRITE 1

/* Main-68000 data bus bytes in $200000-$23FFFF while SegaCD is active.
 * Includes the 1M linear and cell-arranged views, in either bank. Instruction
 * fetches, other CPUs, DMA and debugger inspection are excluded. */
void MainWordRAMTransfer_Reset(void);
void MainWordRAMTransfer_BeginFrame(void);
void MainWordRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int MainWordRAMTransfer_GetHistory(int write, unsigned long *values);
unsigned long MainWordRAMTransfer_GetBytesPerSecond(int write);

/* Native main-CPU data accesses and BIOS HLE only. Raw M68K_RB/RW/WB/WW
 * remain available for inspection and accesses by other bus masters. */
unsigned char M68K_Data_RB(unsigned int address);
unsigned short M68K_Data_RW(unsigned int address);
void M68K_Data_WB(unsigned int address, unsigned char value);
void M68K_Data_WW(unsigned int address, unsigned short value);

#ifdef __cplusplus
}
#endif
#endif
