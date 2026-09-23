#ifndef CD_HLE_H
#define CD_HLE_H
#ifdef __cplusplus
extern "C" {
#endif
extern int CD_HLE_Active;
int CD_HLE_Prepare(void);
void CD_HLE_Close(void);
int CD_HLE_Boot(void);
void CD_HLE_Frame(void);
int CD_HLE_AudioPlaying(void);
unsigned CD_HLE_MainExec(int cycles);
unsigned CD_HLE_SubExec(int cycles);
#ifdef __cplusplus
}
#endif
#endif
