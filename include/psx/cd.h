#ifndef FFT_PSX_CD_H
#define FFT_PSX_CD_H

#include "psx/types.h"

#define CdlSetmode   0x0e
#define CdlSeekL     0x15
#define CdlModeSpeed 0x80

#define CdlComplete  0x02
#define CdlDiskError 0x05

/* Psy-Q CdlATV: CD audio to SPU attenuation (left to left, left to right,
 * right to right, right to left). */
typedef struct {
    u8 val0;
    u8 val1;
    u8 val2;
    u8 val3;
} CdlATV;

extern void CdInit(void);
extern void CdSetDebug(int);
extern void SetMem(int);
extern void CdReadyCallback(void*);
extern void CdReadCallback(void*);
extern int CdControl(u8, u8*, u8*);
extern int CdControlF(u8, u8*);
extern int CdSync(int, u8*);
extern int CdRead(int, u32*, int);
extern int CdReadSync(int, u8*);
extern int CdReady(int, u8*);
extern int CdGetSector(void*, int);
extern int CdDataSync(int);

s32 CdMix(CdlATV* vol);
s32 CdControlb(s32 command, const u8* parameter, u8* result);
s32 CdRead2(s32 mode);
s32 StGetBacklog(s32* backlog);
void CdFlush(void);
void CdIntToPos(s32, void*);
void CdReset(int mode);
void StCdInterrupt(void);
void StFreeRing(void* frame_data);
void StRingStatus(s16* free_sectors, s16* read_sectors);
void StSetDataReadyCallback(void);
void StSetRing(void* buffer, s32 sectors);
void StSetStream(s32 mode, s32 start_frame, s32 end_frame, void* start_callback, void* end_callback);
void StSetMask(s32 channel, s32 mask, s32 end_frame);
s32 CdPosToInt(u8* position);
s32 CdStatus(void);

#endif
