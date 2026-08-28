#ifndef FFT_PSX_ETC_H
#define FFT_PSX_ETC_H

#include "psx/types.h"

extern int VSync(int);
extern void ResetCallback(void);
extern void VSyncCallback(void*);
extern void PadInit(int);
extern u32 PadRead(s32);

/* Psy-Q kernel.h event descriptors (DescHW|0x11, DescSW|0x01), event specs
 * and event modes, as passed to OpenEvent by the memory-card setup. */
#define HwCARD     0xF0000011
#define SwCARD     0xF4000001
#define EvSpIOE    0x0004
#define EvSpTIMOUT 0x0100
#define EvSpNEW    0x2000
#define EvSpERROR  0x8000
#define EvMdNOINTR 0x2000

/* Psy-Q kernel.h memory-card directory entry (firstfile/nextfile). */
typedef struct DIRENTRY {
    char name[20];
    int attr;
    int size;
    struct DIRENTRY* next;
    int head;
    char system[4];
} DIRENTRY;

s32 EnableEvent(s32 event);
s32 GetVideoMode(void);
void ChangeClearPad(s32 val);
void PadStop(void);
void StopCallback(void);

DIRENTRY* firstfile(char* pattern, DIRENTRY* entry);
DIRENTRY* nextfile(DIRENTRY* entry);

#endif
