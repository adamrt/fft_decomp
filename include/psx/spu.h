#ifndef FFT_PSX_SPU_H
#define FFT_PSX_SPU_H

#include "psx/types.h"

/* Clean-room declarations of the Psy-Q LIBSPU interface that the linked
 * library (0x800186c4-0x8001bb5c) exports to the Suzuki sound driver. Names,
 * layouts and signatures follow the Psy-Q library reference; the offsets
 * noted on each type are the ones the driver's callers use. */

/* Psy-Q SpuVolume: a left/right volume pair. */
typedef struct {
    s16 left;  /* 0x00 */
    s16 right; /* 0x02 */
} SpuVolume;

/* Psy-Q SpuReverbAttr (0x14 bytes). The driver keeps one at 0x80037008 for
 * SpuSetReverbModeParam and SpuSetReverbDepth. */
typedef struct {
    u32 mask;        /* 0x00 */
    s32 mode;        /* 0x04 */
    SpuVolume depth; /* 0x08 */
    s32 delay;       /* 0x0c */
    s32 feedback;    /* 0x10 */
} SpuReverbAttr;

/* Psy-Q SpuExtAttr: CD or external input settings of SpuCommonAttr. */
typedef struct {
    SpuVolume volume; /* 0x00 */
    s32 reverb;       /* 0x04 */
    s32 mix;          /* 0x08 */
} SpuExtAttr;

/* Psy-Q SpuCommonAttr (0x28 bytes); the driver's copy is at 0x80037020. */
typedef struct {
    u32 mask;           /* 0x00 */
    SpuVolume mvol;     /* 0x04 */
    SpuVolume mvolmode; /* 0x08 */
    SpuVolume mvolx;    /* 0x0c */
    SpuExtAttr cd;      /* 0x10 */
    SpuExtAttr ext;     /* 0x1c */
} SpuCommonAttr;

/* Psy-Q SpuVoiceAttr (0x40 bytes). 0x80014180 fills addr and the envelope
 * fields from a WAVESET instrument entry. */
typedef struct {
    u32 voice;         /* 0x00 */
    u32 mask;          /* 0x04 */
    SpuVolume volume;  /* 0x08 */
    SpuVolume volmode; /* 0x0c */
    SpuVolume volumex; /* 0x10 */
    u16 pitch;         /* 0x14 */
    u16 note;          /* 0x16 */
    u16 sample_note;   /* 0x18 */
    s16 envx;          /* 0x1a */
    u32 addr;          /* 0x1c */
    u32 loop_addr;     /* 0x20 */
    s32 a_mode;        /* 0x24 */
    s32 s_mode;        /* 0x28 */
    s32 r_mode;        /* 0x2c */
    u16 ar;            /* 0x30 */
    u16 dr;            /* 0x32 */
    u16 sr;            /* 0x34 */
    u16 rr;            /* 0x36 */
    u16 sl;            /* 0x38 */
    u16 adsr1;         /* 0x3a */
    u16 adsr2;         /* 0x3c */
} SpuVoiceAttr;

/* Psy-Q SpuDecodedData: the SPU's decoded CD and voice 1/3 capture buffers.
 * SpuReadDecodedData (0x8001ac7c) offsets voice1 by 0x800 bytes. */
typedef struct {
    s16 cd_left[0x200];
    s16 cd_right[0x200];
    s16 voice1[0x200];
    s16 voice3[0x200];
} SpuDecodedData;

typedef void (*SpuTransferCallbackProc)(void);

extern void SsUtReverbOff(void);
extern void SsUtReverbOn(void);

void SpuFree(u32 addr);
s32 SpuMalloc(s32 size);
void SpuQuit(void);
void _spu_Fr(void* addr, u32 size);

/* libspu internals. */
extern s32 _spu_inTransfer;
extern volatile s32 _spu_rev_attr_delay;
extern volatile u16 _spu_rev_attr_depth_left;
extern volatile u16 _spu_rev_attr_depth_right;
extern volatile s32 _spu_rev_attr_feedback;
extern volatile s32 _spu_rev_attr_mode;
extern void (*_spu_transferCallback)(void);
void _SpuInit(s32 hot);
u32 _SpuSetAnyVoice(s32 on_off, u32 voice_bit, s32 reg_index_low, s32 reg_index_high);

/* 0x800194c4-0x8001b4b0: entry points used by the Suzuki driver. */
s32 SpuInitMalloc(s32 num, u8* top);
u32 SpuSetNoiseVoice(s32 on_off, u32 voice_bit);
s32 SpuSetNoiseClock(s32 n_clock);
u32 SpuRead(u8* addr, u32 size);
s32 SpuSetReverb(s32 on_off);
void SpuSetReverbModeParam(SpuReverbAttr* attr);
void SpuGetReverbModeParam(SpuReverbAttr* attr);
void SpuSetReverbDepth(SpuReverbAttr* attr);
s32 SpuReadDecodedData(SpuDecodedData* d_data, s32 flag);
void SpuSetKey(s32 on_off, u32 voice_bit);
/* The target stores the key status with `sh`; the documented pointer type
 * is long. */
void SpuGetVoiceEnvelopeAttr(s32 v_num, s32* key_stat, s16* envx);
u32 SpuWrite(u8* addr, u32 size);
u32 SpuSetTransferStartAddr(u32 addr);
s32 SpuSetTransferMode(s32 mode);
SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func);
u32 SpuSetPitchLFOVoice(s32 on_off, u32 voice_bit);
void SpuSetCommonAttr(SpuCommonAttr* attr);
u32 SpuSetReverbVoice(s32 on_off, u32 voice_bit);

/* Individual voice attribute setters (0x8001b428-0x8001bb58). */
void SpuSetVoiceVolume(s32 v_num, s16 vol_l, s16 vol_r);
void SpuSetVoiceVolumeAttr(s32 v_num, s16 vol_l, s16 vol_r, s16 vol_mode_l, s16 vol_mode_r);
void SpuSetVoicePitch(s32 v_num, u16 pitch);
void SpuSetVoiceStartAddr(s32 v_num, u32 start_addr);
void SpuSetVoiceLoopStartAddr(s32 v_num, u32 loop_start_addr);
void SpuSetVoiceDR(s32 v_num, u16 dr);
void SpuSetVoiceRR(s32 v_num, u16 rr);
void SpuSetVoiceSL(s32 v_num, u16 sl);
void SpuSetVoiceARAttr(s32 v_num, u16 ar, s32 ar_mode);
void SpuSetVoiceSRAttr(s32 v_num, u16 sr, s32 sr_mode);
void SpuSetVoiceRRAttr(s32 v_num, u16 rr, s32 rr_mode);

#endif
