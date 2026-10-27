#ifndef WORD_RAM_TRANSFER_H
#define WORD_RAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif

#define WORD_RAM_TRANSFER_HISTORY 256
#define WORD_RAM_TRANSFER_READ 0
#define WORD_RAM_TRANSFER_WRITE 1

/* Sub-68000 data bus bytes, not instruction fetches or physical RAM changes.
 * Includes 1M dot-image accesses (one byte per pixel access), even when a
 * priority mask suppresses a write. DMA and debugger accesses are excluded. */
void WordRAMTransfer_Reset(void);
void WordRAMTransfer_BeginFrame(void);
void WordRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int WordRAMTransfer_GetHistory(int write, unsigned long *values);
unsigned long WordRAMTransfer_GetBytesPerSecond(int write);

/* Measure both Program RAM and Word RAM CPU data accesses (including HLE).
 * Raw S68K_RB/RW/WB/WW remain available for debugger inspection. */
unsigned char S68K_Data_RB(unsigned int address);
unsigned short S68K_Data_RW(unsigned int address);
void S68K_Data_WB(unsigned int address, unsigned char value);
void S68K_Data_WW(unsigned int address, unsigned short value);

#ifdef __cplusplus
}
#endif
#endif
