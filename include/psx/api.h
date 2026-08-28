#ifndef FFT_PSX_API_H
#define FFT_PSX_API_H

#include "psx/types.h"

s32 FileClose(s32 descriptor);
s32 FileGetError(s32 descriptor);
s32 FileOpen(const char* path, s32 mode);
s32 FileRead(s32 descriptor, void* destination, s32 size);
s32 FileSeek(s32 descriptor, s32 offset, s32 origin);
s32 FileWrite(s32 descriptor, const void* source, s32 size);
s32 CloseEvent(s32 event);
s32 OpenEvent(s32 desc, s32 spec, s32 mode, s32 func);
s32 SetRCnt(u32 spec, u16 target, s32 mode);
s32 StartRCnt(u32 spec);
s32 TestEvent(s32 event);
s32 DisableEvent(s32 event);
void EnterCriticalSection(void);
void ExitCriticalSection(void);
void StartCARD_earlysafe(void);
void _bu_init(void);
void _card_auto(s32 val);

s32 FileDelete(const char* path);
s32 FileFormat(const char* path);

#endif
