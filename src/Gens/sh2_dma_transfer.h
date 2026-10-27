#ifndef SH2_DMA_TRANSFER_H
#define SH2_DMA_TRANSFER_H
#include "SH2.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SH2_DMA_TRANSFER_HISTORY 256
#define SH2_DMA_TRANSFER_READ 0
#define SH2_DMA_TRANSFER_WRITE 1
enum { SH2_DMA_MASTER, SH2_DMA_SLAVE, SH2_DMA_TRANSFER_KINDS };

/* DMAC source reads/destination writes, aggregated across channels 0 and 1
 * separately for each CPU. Counts issued bus payload bytes, including fixed
 * addresses and writes suppressed by destination handlers. CPU accesses,
 * register programming, pending requests and other DMA engines are excluded. */
void SH2DMATransfer_Reset(void);
void SH2DMATransfer_ClearCPU(SH2_CONTEXT *cpu);
void SH2DMATransfer_BeginFrame(void);
void SH2DMATransfer_EndFrame(unsigned int frames_per_second);
unsigned int SH2DMATransfer_GetHistory(int cpu, int write, unsigned long *values);
unsigned long SH2DMATransfer_GetBytesPerSecond(int cpu, int write);
/* Called once per completed read/write pair by the SH2 DMA loops. */
void SH2DMATransfer_AddBytes(SH2_CONTEXT *cpu, unsigned long bytes);
#ifdef __cplusplus
}
#endif
#endif
