#ifndef SDRAM_TRANSFER_H
#define SDRAM_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif
#define SDRAM_TRANSFER_HISTORY 256
#define SDRAM_TRANSFER_READ 0
#define SDRAM_TRANSFER_WRITE 1
/* 32X main RAM (256 KiB): both SH2 CPUs' data accesses and DMAC payload.
 * Instruction fetch, debugger access and host initialization/state loading are excluded.
 * Repeated accesses count each time, including mirrored SDRAM addresses. */
void SDRAMTransfer_Reset(void);
void SDRAMTransfer_BeginFrame(void);
void SDRAMTransfer_EndFrame(unsigned int frames_per_second);
unsigned int SDRAMTransfer_GetHistory(int write, unsigned long *values);
unsigned long SDRAMTransfer_GetBytesPerSecond(int write);
#ifdef __cplusplus
}
#endif
#endif
