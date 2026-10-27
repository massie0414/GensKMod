#ifndef VRAM_TRANSFER_H
#define VRAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif

#define VRAM_TRANSFER_HISTORY 256
#define VRAM_TRANSFER_READ 0
#define VRAM_TRANSFER_WRITE 1

enum {
    VDP_TRANSFER_VRAM, VDP_TRANSFER_CRAM, VDP_TRANSFER_VSRAM,
    VDP_TRANSFER_DMA, VDP_TRANSFER_KINDS
};

/* These frame/reset methods sample all four VDP transfer channels together. */
/* Main-68000 CPU bus bytes transferred through the Genesis VDP data port
 * with VRAM selected. CRAM/VSRAM, VDP DMA (including fill setup), other CPUs and
 * debugger accesses are excluded. Byte writes count as one CPU byte even
 * though the VDP duplicates it; unsupported byte reads do not access VRAM. */
void VRAMTransfer_Reset(void);
void VRAMTransfer_BeginFrame(void);
void VRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int VRAMTransfer_GetHistory(int write, unsigned long *values);
unsigned long VRAMTransfer_GetBytesPerSecond(int write);

/* CRAM/VSRAM measure CPU port bytes; DMA measures source reads and destination
 * writes performed by the VDP engine (memory transfer, VRAM copy and fill).
 * HLE CPU copies stay in the CPU channel, not the DMA channel. */
unsigned int VDPTransfer_GetHistory(int kind, int write, unsigned long *values);
unsigned long VDPTransfer_GetBytesPerSecond(int kind, int write);
void VDPTransfer_RecordDMA(unsigned long reads, unsigned long writes);
/* HLE palette/scroll helpers that update the backing store directly. */
void VDPTransfer_RecordHLEWrite(int kind, unsigned long bytes);

/* Register-preserving continuation of M68K_Data_*; call those public CPU
 * data-access helpers for native/HLE transfers, and raw M68K_* for inspection. */
unsigned short M68K_VRAM_RW(unsigned int address);
void M68K_VRAM_WB(unsigned int address, unsigned char value);
void M68K_VRAM_WW(unsigned int address, unsigned short value);

#ifdef __cplusplus
}
#endif
#endif
