#ifndef PWM_FIFO_TRANSFER_H
#define PWM_FIFO_TRANSFER_H
#ifdef __cplusplus
extern "C" {
#endif
#define PWM_FIFO_TRANSFER_HISTORY 256
#define PWM_FIFO_TRANSFER_READ 0
#define PWM_FIFO_TRANSFER_WRITE 1
/* Combined L/R FIFO payload. Each actual enqueue or playback dequeue is
 * two bytes. Mono writes enqueue into both FIFOs (four bytes). Byte writes
 * count when the low-byte write commits a sample, not while staging high bits.
 * Full/empty attempts, status polls and output mixing are excluded. */
void PWMFIFOTransfer_Reset(void);
void PWMFIFOTransfer_BeginFrame(void);
void PWMFIFOTransfer_EndFrame(unsigned int frames_per_second);
unsigned int PWMFIFOTransfer_GetHistory(int write, unsigned long *values);
unsigned long PWMFIFOTransfer_GetBytesPerSecond(int write);
#ifdef __cplusplus
}
#endif
#endif
