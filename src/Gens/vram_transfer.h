#ifndef VRAM_TRANSFER_H
#define VRAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif

#define VRAM_TRANSFER_HISTORY 256
#define VRAM_TRANSFER_READ 0
#define VRAM_TRANSFER_WRITE 1

/* Main-68000 CPU bus bytes transferred through the Genesis VDP data port
 * with VRAM selected. CRAM/VSRAM, VDP DMA (including fill setup), other CPUs and
 * debugger accesses are excluded. Byte writes count as one CPU byte even
 * though the VDP duplicates it; unsupported byte reads do not access VRAM. */
void VRAMTransfer_Reset(void);
void VRAMTransfer_BeginFrame(void);
void VRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int VRAMTransfer_GetHistory(int write, unsigned long *values);
unsigned long VRAMTransfer_GetBytesPerSecond(int write);

/* Register-preserving continuation of M68K_Data_*; call those public CPU
 * data-access helpers for native/HLE transfers, and raw M68K_* for inspection. */
unsigned short M68K_VRAM_RW(unsigned int address);
void M68K_VRAM_WB(unsigned int address, unsigned char value);
void M68K_VRAM_WW(unsigned int address, unsigned short value);

#ifdef __cplusplus
}
#endif
#endif
