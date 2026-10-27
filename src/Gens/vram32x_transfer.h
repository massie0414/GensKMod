#ifndef VRAM32X_TRANSFER_H
#define VRAM32X_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif
#define VRAM32X_TRANSFER_HISTORY 256
/* Stored bytes across both 128 KiB framebuffers: CPU, SH2 DMA and autofill.
 * Repeated/unchanged writes count; transparent bytes and rejected writes do not.
 * Framebuffer reads, palette RAM, host initialization and state imports do not count. */
void VRAM32XTransfer_Reset(void);
void VRAM32XTransfer_BeginFrame(void);
void VRAM32XTransfer_EndFrame(unsigned int frames_per_second);
unsigned int VRAM32XTransfer_GetHistory(unsigned long *values);
unsigned long VRAM32XTransfer_GetBytesPerSecond(void);
#ifdef __cplusplus
}
#endif
#endif
