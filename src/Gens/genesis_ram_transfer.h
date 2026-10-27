#ifndef GENESIS_RAM_TRANSFER_H
#define GENESIS_RAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif

#define GENESIS_RAM_TRANSFER_HISTORY 256
#define GENESIS_RAM_TRANSFER_READ 0
#define GENESIS_RAM_TRANSFER_WRITE 1
enum { GENESIS_RAM_MAIN, GENESIS_RAM_SUB, GENESIS_RAM_TRANSFER_KINDS };

/* Main-68000 data transfers to main RAM (64 KiB) and Z80 RAM (8 KiB).
 * Includes mirrors and HLE data accesses. Instruction/immediate fetches,
 * other CPUs, DMA, debugger and rejected Z80-bus accesses are excluded.
 * Z80 RAM counts one actual byte per accepted byte/word access. */
void GenesisRAMTransfer_Reset(void);
void GenesisRAMTransfer_BeginFrame(void);
void GenesisRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int GenesisRAMTransfer_GetHistory(int kind, int write, unsigned long *values);
unsigned long GenesisRAMTransfer_GetBytesPerSecond(int kind, int write);
void GenesisRAMTransfer_AddMainBytes(int write, unsigned long bytes);

/* Continuations of the public M68K_Data_* helpers. */
unsigned char M68K_RAM_RB(unsigned int address);
unsigned short M68K_RAM_RW(unsigned int address);
void M68K_RAM_WB(unsigned int address, unsigned char value);
void M68K_RAM_WW(unsigned int address, unsigned short value);
#ifdef __cplusplus
}
#endif
#endif
