#ifndef PSX_LIBSPU_H
#define PSX_LIBSPU_H

#include "psx/types.h"

/* Clean-room declarations of the Psy-Q LIBSPU interface that the linked
 * library (0x800186c4-0x8001bb5c) exports to the Suzuki sound driver. Names,
 * layouts and signatures follow the Psy-Q library reference; the offsets
 * noted on each type are the ones the driver's callers use. */

/* Psy-Q on/off selectors. */
enum {
    SPU_OFF = 0,
    SPU_ON = 1,
};

/* All 24 SPU voice channels. */
enum { SPU_ALLCH = 0x00ffffff };

#define SPU_KEYCH(x)   (0x1L << (x))
#define SPU_VOICECH(x) SPU_KEYCH(x)

/* SpuReadDecodedData capture selection. */
enum {
    SPU_ALL = 0,
    SPU_CDONLY = 5,
    SPU_VOICEONLY = 6,
};

/* SpuEnv.mask selects event queueing; the driver uses the same bit in _spu_env. */
enum { SPU_ENV_EVENT_QUEUEING = 0x1 };

/* SpuFlush deferred-operation mask. */
enum { SPU_EVENT_KEY = 0x1 };

/* SpuSetTransferMode selectors. */
enum {
    SPU_TRANSFER_BY_DMA = 0,
    SPU_TRANSFER_BY_IO = 1,
};

/* Psy-Q volume modes, shared by master and voice volume attributes. */
typedef enum {
    SPU_VOICE_DIRECT = 0,
    SPU_VOICE_LINEARIncN = 1,
    SPU_VOICE_LINEARIncR = 2,
    SPU_VOICE_LINEARDecN = 3,
    SPU_VOICE_LINEARDecR = 4,
    SPU_VOICE_EXPIncN = 5,
    SPU_VOICE_EXPIncR = 6,
    SPU_VOICE_EXPDec = 7,
} psyq_spu_volume_mode_e;

/* SpuCommonAttr.mask; zero selects every supported attribute. */
typedef enum {
    SPU_COMMON_MVOLL = 0x0001,
    SPU_COMMON_MVOLR = 0x0002,
    SPU_COMMON_MVOLMODEL = 0x0004,
    SPU_COMMON_MVOLMODER = 0x0008,
    SPU_COMMON_CDVOLL = 0x0040,
    SPU_COMMON_CDVOLR = 0x0080,
    SPU_COMMON_CDREV = 0x0100,
    SPU_COMMON_CDMIX = 0x0200,
    SPU_COMMON_EXTVOLL = 0x0400,
    SPU_COMMON_EXTVOLR = 0x0800,
    SPU_COMMON_EXTREV = 0x1000,
    SPU_COMMON_EXTMIX = 0x2000,
} psyq_spu_common_attribute_e;

/* SpuVoiceAttr.mask. */
typedef enum {
    SPU_VOICE_VOLL = 0x00001,
    SPU_VOICE_VOLR = 0x00002,
    SPU_VOICE_VOLMODEL = 0x00004,
    SPU_VOICE_VOLMODER = 0x00008,
    SPU_VOICE_PITCH = 0x00010,
    SPU_VOICE_NOTE = 0x00020,
    SPU_VOICE_SAMPLE_NOTE = 0x00040,
    SPU_VOICE_WDSA = 0x00080,
    SPU_VOICE_ADSR_AMODE = 0x00100,
    SPU_VOICE_ADSR_SMODE = 0x00200,
    SPU_VOICE_ADSR_RMODE = 0x00400,
    SPU_VOICE_ADSR_AR = 0x00800,
    SPU_VOICE_ADSR_DR = 0x01000,
    SPU_VOICE_ADSR_SR = 0x02000,
    SPU_VOICE_ADSR_RR = 0x04000,
    SPU_VOICE_ADSR_SL = 0x08000,
    SPU_VOICE_LSAX = 0x10000,
    SPU_VOICE_ADSR_ADSR1 = 0x20000,
    SPU_VOICE_ADSR_ADSR2 = 0x40000,
} psyq_spu_voice_attribute_e;

/* SpuReverbAttr.mask; zero selects every attribute. */
typedef enum {
    SPU_REV_MODE = 0x01,
    SPU_REV_DEPTHL = 0x02,
    SPU_REV_DEPTHR = 0x04,
    SPU_REV_DELAYTIME = 0x08,
    SPU_REV_FEEDBACK = 0x10,
} psyq_spu_reverb_attribute_e;

/* SpuReverbAttr.mode selectors; the clear flag is independent of the mode. */
typedef enum {
    SPU_REV_MODE_OFF = 0,
    SPU_REV_MODE_ROOM = 1,
    SPU_REV_MODE_STUDIO_A = 2,
    SPU_REV_MODE_STUDIO_B = 3,
    SPU_REV_MODE_STUDIO_C = 4,
    SPU_REV_MODE_HALL = 5,
    SPU_REV_MODE_SPACE = 6,
    SPU_REV_MODE_ECHO = 7,
    SPU_REV_MODE_DELAY = 8,
    SPU_REV_MODE_PIPE = 9,
} psyq_spu_reverb_mode_e;

enum {
    SPU_REV_MODE_CLEAR_WA = 0x100,
    PSYQ_SPU_REVERB_MODE_COUNT = 10,
};

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

#define SPU_DECODEDDATA_SIZE 0x200
#define SPU_DECODEDATA_SIZE  SPU_DECODEDDATA_SIZE

/* Psy-Q SpuDecodedData: the SPU's decoded CD and voice 1/3 capture buffers.
 * SpuReadDecodedData (0x8001ac7c) offsets voice1 by 0x800 bytes. */
typedef struct {
    s16 cd_left[SPU_DECODEDDATA_SIZE];
    s16 cd_right[SPU_DECODEDDATA_SIZE];
    s16 voice1[SPU_DECODEDDATA_SIZE];
    s16 voice3[SPU_DECODEDDATA_SIZE];
} SpuDecodedData;

typedef struct psyq_spu_heap_record {
    u32 address;
    u32 size;
} psyq_spu_heap_record_t;

typedef void (*SpuTransferCallbackProc)(void);

extern void SpuInit(void);

void SpuFree(u32 addr);
s32 SpuMalloc(s32 size);
void SpuQuit(void);
u32 _spu_Fr(void* addr, u32 size);

/* libspu internals. */
extern s32 _spu_inTransfer;
extern SpuReverbAttr _spu_rev_attr; /* current reverb settings; mask is never written */
extern void (*volatile _spu_transferCallback)(void);
void _SpuInit(s32 hot);
u32 _SpuSetAnyVoice(s32 on_off, u32 voice_bit, s32 reg_index_low, s32 reg_index_high);

/* 0x800194c4-0x8001b4b0: entry points used by the Suzuki driver. */
s32 SpuInitMalloc(s32 num, char* top);
u32 SpuSetNoiseVoice(s32 on_off, u32 voice_bit);
s32 SpuSetNoiseClock(s32 n_clock);
u32 SpuRead(u8* addr, u32 size);
s32 SpuSetReverb(s32 on_off);
s32 SpuSetReverbModeParam(SpuReverbAttr* attr);
void SpuGetReverbModeParam(SpuReverbAttr* attr);
s32 SpuSetReverbDepth(SpuReverbAttr* attr);
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

void SpuStart(void);
void SpuInitHot(void);
s32 SpuClearReverbWorkArea(s32 mode);

/* driver internals */

/* DMA/FIFO control values and polling units in the linked driver. */
#define PSYQ_SPU_POLL_LIMIT              3840
#define PSYQ_SPU_FIFO_CHUNK_BYTES        64
#define PSYQ_SPU_STATUS_MASK             0x7ff
#define PSYQ_SPU_STATUS_TRANSFER_BUSY    0x400
#define PSYQ_SPU_TRANSFER_MODE_MASK      0x30
#define PSYQ_SPU_TRANSFER_MODE_CLEAR     0xffcf
#define PSYQ_SPU_TRANSFER_MODE_IO        0x10
#define PSYQ_SPU_TRANSFER_MODE_DMA_WRITE 0x20
#define PSYQ_SPU_TRANSFER_MODE_DMA_READ  0x30

/* Private DMA operation selectors and 16-bit register indices. */
typedef enum {
    PSYQ_SPU_DMA_READ = 0,
    PSYQ_SPU_DMA_WRITE = 1,
    PSYQ_SPU_DMA_SET_ADDRESS = 2,
    PSYQ_SPU_DMA_START_TRANSFER = 3,
} psyq_spu_dma_operation_e;

typedef enum {
    PSYQ_SPU_REG_KEY_ON_LOW = 0xc4,
    PSYQ_SPU_REG_KEY_ON_HIGH = 0xc5,
    PSYQ_SPU_REG_KEY_OFF_LOW = 0xc6,
    PSYQ_SPU_REG_KEY_OFF_HIGH = 0xc7,
    PSYQ_SPU_REG_PITCH_LFO_LOW = 0xc8,
    PSYQ_SPU_REG_PITCH_LFO_HIGH = 0xc9,
    PSYQ_SPU_REG_NOISE_LOW = 0xca,
    PSYQ_SPU_REG_NOISE_HIGH = 0xcb,
    PSYQ_SPU_REG_REVERB_LOW = 0xcc,
    PSYQ_SPU_REG_REVERB_HIGH = 0xcd,
    PSYQ_SPU_REG_REVERB_START = 0xd1,
} psyq_spu_register_index_e;

typedef enum {
    PSYQ_SPU_ADDRESS_AS_UNITS = -1,
    PSYQ_SPU_ADDRESS_ALIGNED_BYTES = -2,
    PSYQ_SPU_REGISTER_RAW = 0,
    PSYQ_SPU_REGISTER_BYTE_ADDRESS = 1,
} psyq_spu_address_format_e;

#define PSYQ_SPU_HEAP_FREE         0x80000000U
#define PSYQ_SPU_HEAP_TAIL         0x40000000U
#define PSYQ_SPU_HEAP_DELETED      0x2fffffffU
#define PSYQ_SPU_HEAP_ADDRESS_MASK 0x0fffffffU
#define PSYQ_SPU_HEAP_START        0x1010U

/* Register layout reached through LIBSPU's pointer at 0x8002ad44. */
typedef struct psyq_spu_voice {
    u16 volume_left;
    u16 volume_right;
    u16 pitch;
    u16 start_address;
    u16 adsr1;
    u16 adsr2;
    u16 envelope;
    u16 loop_address;
} psyq_spu_voice_t;

/* SPU 16-bit reverb configuration registers at 0x1f801dc0..0x1f801dff.
 * Addresses and APF delays use eight-byte SPU RAM units; volumes hold 1.15 signed bits. */
typedef struct psyq_spu_reverb_registers {
    u16 apf1_delay;                  /* 0x00; dAPF1 */
    u16 apf2_delay;                  /* 0x02; dAPF2 */
    u16 reflection_volume1;          /* 0x04; vIIR */
    u16 comb1_volume;                /* 0x06; vCOMB1 */
    u16 comb2_volume;                /* 0x08; vCOMB2 */
    u16 comb3_volume;                /* 0x0a; vCOMB3 */
    u16 comb4_volume;                /* 0x0c; vCOMB4 */
    u16 reflection_volume2;          /* 0x0e; vWALL */
    u16 apf1_volume;                 /* 0x10; vAPF1 */
    u16 apf2_volume;                 /* 0x12; vAPF2 */
    u16 same_reflection1_left;       /* 0x14; mLSAME */
    u16 same_reflection1_right;      /* 0x16; mRSAME */
    u16 comb1_left;                  /* 0x18; mLCOMB1 */
    u16 comb1_right;                 /* 0x1a; mRCOMB1 */
    u16 comb2_left;                  /* 0x1c; mLCOMB2 */
    u16 comb2_right;                 /* 0x1e; mRCOMB2 */
    u16 same_reflection2_left;       /* 0x20; dLSAME */
    u16 same_reflection2_right;      /* 0x22; dRSAME */
    u16 different_reflection1_left;  /* 0x24; mLDIFF */
    u16 different_reflection1_right; /* 0x26; mRDIFF */
    u16 comb3_left;                  /* 0x28; mLCOMB3 */
    u16 comb3_right;                 /* 0x2a; mRCOMB3 */
    u16 comb4_left;                  /* 0x2c; mLCOMB4 */
    u16 comb4_right;                 /* 0x2e; mRCOMB4 */
    u16 different_reflection2_left;  /* 0x30; dLDIFF */
    u16 different_reflection2_right; /* 0x32; dRDIFF */
    u16 apf1_left;                   /* 0x34; mLAPF1 */
    u16 apf1_right;                  /* 0x36; mRAPF1 */
    u16 apf2_left;                   /* 0x38; mLAPF2 */
    u16 apf2_right;                  /* 0x3a; mRAPF2 */
    u16 input_volume_left;           /* 0x3c; vLIN */
    u16 input_volume_right;          /* 0x3e; vRIN */
} psyq_spu_reverb_registers_t;

typedef char assert_psyq_spu_reverb_register_size[(sizeof(psyq_spu_reverb_registers_t) == 64) ? 1 : -1];

typedef struct psyq_spu_registers {
    psyq_spu_voice_t voices[24]; /* 0x000 */
    u16 main_volume_left;        /* 0x180 */
    u16 main_volume_right;
    u16 reverb_depth_left;
    u16 reverb_depth_right;
    u16 key_on_low; /* 0x188 */
    u16 key_on_high;
    u16 key_off_low;
    u16 key_off_high;
    u16 pitch_lfo_low; /* 0x190 */
    u16 pitch_lfo_high;
    u16 noise_low;
    u16 noise_high;
    u16 reverb_low;
    u16 reverb_high;
    u16 voice_status_low;
    u16 voice_status_high;
    u16 _unused_1a0;
    u16 reverb_start;
    u16 irq_address;
    u16 transfer_address;
    u16 transfer_fifo;
    u16 control;
    u16 transfer_control;
    u16 status;
    u16 cd_volume_left; /* 0x1b0 */
    u16 cd_volume_right;
    u16 external_volume_left;
    u16 external_volume_right;
    u16 current_main_volume_left;
    u16 current_main_volume_right;
    u16 _unused_1bc;
    u16 _unused_1be;
    psyq_spu_reverb_registers_t reverb; /* 0x1c0 */
} psyq_spu_registers_t;

typedef char assert_psyq_spu_voice_size[(sizeof(psyq_spu_voice_t) == 16) ? 1 : -1];
typedef char assert_psyq_spu_register_size[(sizeof(psyq_spu_registers_t) == 512) ? 1 : -1];

/* The library addresses the block through the named layout and as raw 16-bit registers. */
typedef union psyq_spu_register_block {
    psyq_spu_registers_t rxx;
    u16 raw[0x100];
} psyq_spu_register_block_t;

extern volatile psyq_spu_register_block_t* _spu_RXX;

extern volatile u32* g_psyq_spu_dma_address_register;  /* DMA4 MADR */
extern volatile u32* g_psyq_spu_dma_block_register;    /* DMA4 BCR */
extern volatile u32* g_psyq_spu_dma_control_register;  /* DMA4 CHCR */
extern volatile u32* g_psyq_spu_dma_priority_register; /* DMA priority: DPCR */
extern volatile u32* g_psyq_spu_delay_register;
extern u16 _spu_tsa;       /* transfer address, in eight-byte SPU units */
extern s32 _spu_transMode; /* normalized transfer mode: 0 DMA, 1 programmed I/O */
extern s32 _spu_addrMode;
extern s32 _spu_mem_mode;       /* address alignment enabled */
extern s32 _spu_mem_mode_plus;  /* byte-address shift */
extern s32 _spu_mem_mode_unit;  /* address alignment */
extern s32 _spu_mem_mode_unitM; /* alignment mask */
extern void (*volatile _spu_IRQCallback)(void);
extern u16 g_psyq_spu_init_fifo_data[8]; /* initialization block sent through the FIFO */
extern s32 g_psyq_spu_dma_direction;     /* transfer direction: 0 to SPU, 1 from SPU */
extern void* g_psyq_spu_dma_buffer;
extern u32 g_psyq_spu_dma_blocks;
extern s32 _spu_isCalled;   /* event/callback setup flag */
extern u32 _spu_env;        /* event queueing flags */
extern s32 _spu_EVdma;      /* DMA completion BIOS event */
extern u32 _spu_keystat;    /* keyed-on voices */
extern s32 _spu_trans_mode; /* caller's transfer-mode value */
extern s32 _spu_rev_flag;
extern s32 _spu_rev_reserve_wa; /* reserve reverb work RAM */
extern u32 _spu_rev_offsetaddr; /* reverb work start, in eight-byte units */
extern volatile u32 _spu_RQvoice;
extern volatile u32 _spu_RQmask; /* queued SPU_EVENT_* operations */
extern volatile u16 _spu_RQ[10]; /* deferred key, LFO, noise and reverb masks */

typedef struct psyq_spu_reverb_parameters {
    u32 mask;
    psyq_spu_reverb_registers_t registers;
} psyq_spu_reverb_parameters_t;
extern psyq_spu_reverb_parameters_t _spu_rev_param[PSYQ_SPU_REVERB_MODE_COUNT];
void _spu_setReverbAttr(psyq_spu_reverb_parameters_t* parameters);
extern s32 _spu_AllocBlockNum;
extern s32 _spu_AllocLastNum;
extern psyq_spu_heap_record_t* _spu_memList;
extern u32 _spu_rev_startaddr[PSYQ_SPU_REVERB_MODE_COUNT]; /* reverb work starts, in eight-byte units */
extern u8 _spu_zerobuf[0x400];                             /* zero block used to clear reverb work RAM */
extern u16 _spu_voice_centerNote[24];
extern const char g_psyq_spu_timeout_format[];
extern const char g_psyq_spu_init_timeout_name[];
extern const char g_psyq_spu_fifo_ready_timeout_name[];
extern const char g_psyq_spu_status_restore_timeout_name[];

void _spu_gcSPU(void);
s32 _SpuIsInAllocateArea(u32 address);
s32 _SpuIsInAllocateArea_(u32 address);

s32 _spu_init(s32 hot);
void _spu_writeByIO(const u16* source, u32 size);
void _spu_FiDMA(void);
void _spu_Fr_(void* destination, u16 address, u32 blocks);
s32 _spu_t(s32 operation, ...);
u32 _spu_Fw(void* source, u32 size);
void _spu_FsetRXX(s32 reg_index, u32 value, s32 is_address);
u32 _spu_FgetRXXa(s32 reg_index, s32 raw);
void _spu_FsetPCR(s32 enabled);
void _spu_FsetDelayR(void);
void _spu_FsetDelayW(void);
void _spu_Fw1ts(void);
void _SpuDataCallback(void (*callback)(void));

/* This repeated arithmetic is a real hardware-settling delay in retail. */
#define PSYQ_SPU_REGISTER_DELAY()                                                                                      \
    {                                                                                                                  \
        volatile s32 count;                                                                                            \
        volatile s32 value;                                                                                            \
        value = 1;                                                                                                     \
        for (count = 0; count < 2; count++) {                                                                          \
            value *= 13;                                                                                               \
        }                                                                                                              \
    }

u32 _spu_FsetRXXa(s32 reg_index, u32 address);

#endif
