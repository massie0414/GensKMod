#ifndef CD_TRANSFER_H
#define CD_TRANSFER_H

#ifdef __cplusplus
extern "C" {
#endif

#define CD_TRANSFER_HISTORY 256

/* Data payload delivered by the emulated drive, excluding CD-DA, sector
 * headers/ECC, host prefetch and subsequent CDC-to-RAM copies. */
void CDTransfer_Reset(void);
void CDTransfer_BeginFrame(void);
void CDTransfer_AddBytes(unsigned long bytes);
void CDTransfer_EndFrame(unsigned int frames_per_second);
unsigned int CDTransfer_GetHistory(unsigned long *values);
unsigned long CDTransfer_GetBytesPerSecond(void);

#ifdef __cplusplus
}
#endif
#endif
