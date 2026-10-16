#ifndef PSX_LIBETC_H
#define PSX_LIBETC_H

#include "psx/libapi.h"

#include "psx/libc.h"
#include "psx/types.h"

#define MODE_NTSC 0
#define MODE_PAL  1

/* Scratchpad offsets count 32-bit elements. */
#define getScratchAddr(word_index) ((u32*)0x1f800000 + (word_index))

enum { PSX_SCRATCHPAD_BYTE_COUNT = 1024 };

typedef void (*psyq_interrupt_callback_t)(void);

extern int VSync(int);
extern s32 ResetCallback(void);
extern s32 VSyncCallback(psyq_interrupt_callback_t callback);
extern void PadInit(int);
extern u32 PadRead(s32);

s32 GetVideoMode(void);
void PadStop(void);
s32 StopCallback(void);
psyq_interrupt_callback_t DMACallback(s32 dma_channel, psyq_interrupt_callback_t func);

psyq_interrupt_callback_t InterruptCallback(s32 channel, psyq_interrupt_callback_t callback);
s32 VSyncCallbacks(s32 slot, psyq_interrupt_callback_t callback);
s32 RestartCallback(void);
s32 CheckCallback(void);
s32 GetIntrMask(void);
s32 SetIntrMask(s32 mask);
s32 SetVideoMode(s32 mode);

/* pad state */
extern s32 g_psyq_etc_pad_mode;
extern u32 g_psyq_etc_pad_buttons;

extern s32 g_psyq_etc_video_mode;

typedef psyq_interrupt_callback_t (*psyq_callback_setter_t)(s32, psyq_interrupt_callback_t);

/* Interrupt-source indices used by InterruptCallback and the IRQ dispatcher. */
typedef enum {
    PSYQ_IRQ_VBLANK = 0,
    PSYQ_IRQ_DMA = 3,
    PSYQ_IRQ_TIMER0 = 4,
    PSYQ_IRQ_TIMER1 = 5,
    PSYQ_IRQ_TIMER2 = 6,
} psyq_interrupt_source_e;

/* Callback storage, timer configuration and diagnostic limits. */
enum {
    PSYQ_IRQ_SOURCES = 11,
    PSYQ_ETC_CALLBACK_SLOTS = 8,
    PSYQ_ETC_VBLANK_COUNTER_MODE = 0x107,
    PSYQ_ETC_INTERRUPT_STACK_RESERVED_WORDS = 20,
    PSYQ_ETC_VSYNC_TIMEOUT_SHIFT = 15,
    PSYQ_ETC_UNHANDLED_IRQ_LIMIT = 2048,
    PSYQ_ETC_VBLANK_ROOT_COUNTER = 3,
};

typedef struct {
    u16 initialized;
    u16 in_callback;
    psyq_interrupt_callback_t callbacks[PSYQ_IRQ_SOURCES];
    u16 callback_mask;
    u16 saved_mask;
    u32 saved_dma_priority;
    jmp_buf jump_buffer;
    u32 stack[1024];
} psyq_interrupt_state_t;
extern psyq_interrupt_state_t g_psyq_etc_interrupt_state;
extern volatile u16* g_psyq_etc_irq_status;
extern volatile u32* g_psyq_etc_dma_dpcr;
extern psyq_interrupt_callback_t g_psyq_etc_vblank_callbacks[PSYQ_ETC_CALLBACK_SLOTS];
extern volatile u32* g_psyq_etc_timer1_mode;
extern volatile s32 Vcount; /* Updated by VBlank; retail also performs an unused reload. */
extern volatile u32* g_psyq_etc_dma_dicr;
extern psyq_interrupt_callback_t g_psyq_etc_dma_callbacks[PSYQ_ETC_CALLBACK_SLOTS];
psyq_interrupt_state_t* startIntr(void);
psyq_interrupt_state_t* stopIntr(void);
psyq_interrupt_state_t* restartIntr(void);
void trapIntr(void);
void trapIntrVSync(void);
psyq_callback_setter_t startIntrVSync(void);
psyq_interrupt_callback_t setIntrVSync(s32 slot, psyq_interrupt_callback_t callback);
psyq_callback_setter_t startIntrDMA(void);
void trapIntrDMA(void);
psyq_interrupt_callback_t setIntrDMA(s32 channel, psyq_interrupt_callback_t callback);

typedef struct {
    const char* name;
    psyq_interrupt_callback_t (*dma_callback)(s32, psyq_interrupt_callback_t);
    psyq_callback_setter_t irq_callback;
    psyq_interrupt_state_t* (*reset)(void);
    psyq_interrupt_state_t* (*stop)(void);
    psyq_callback_setter_t vblank_callback;
    psyq_interrupt_state_t* (*restart)(void);
    psyq_interrupt_state_t* state;
} psyq_interrupt_dispatch_t;
extern psyq_interrupt_dispatch_t* g_psyq_etc_dispatch;
extern volatile u16* g_psyq_etc_irq_mask;
void psyq_etc_clear_interrupt_state_words(u32* words, s32 count);
void psyq_etc_clear_vblank_callbacks(psyq_interrupt_callback_t* callbacks, s32 count);
void psyq_etc_clear_dma_callbacks(psyq_interrupt_callback_t* callbacks, s32 count);

extern volatile u32* g_psyq_etc_gpu_status;
extern volatile u32* g_psyq_etc_timer1_count;
extern u32 Hcount;
extern s32 g_psyq_etc_last_vblank;
extern char g_psyq_etc_vsync_timeout_message[];
void v_wait(s32 vblank, s32 frames);

/* interrupt diagnostics */
extern s32 g_psyq_etc_unhandled_irq_count;
extern const char g_psyq_etc_uninitialized_irq_format[];
extern const char g_psyq_etc_unhandled_irq_format[];
extern const char g_psyq_etc_dma_error_format[];
extern const char g_psyq_etc_dma_channel_address_format[];

typedef struct {
    volatile u32 address;
    volatile u32 block_control;
    volatile u32 channel_control;
    u32 _unused_0c;
} psyq_dma_channel_t;
extern psyq_dma_channel_t* g_psyq_etc_dma_channels;
psyq_interrupt_callback_t setIntr(s32 slot, psyq_interrupt_callback_t callback);

/* CPU DMA channel indices, transfer controls and priority enables. */
enum {
    PSYQ_DMA_MDEC_IN_CHANNEL = 0,
    PSYQ_DMA_MDEC_OUT_CHANNEL = 1,
    PSYQ_DMA_GPU_CHANNEL = 2,
    PSYQ_DMA_OTC_CHANNEL = 6,
};

#define PSYQ_DMA_TO_DEVICE         0x00000001U
#define PSYQ_DMA_ADDRESS_DECREMENT 0x00000002U
#define PSYQ_DMA_MODE_REQUEST      0x00000200U
#define PSYQ_DMA_MODE_LINKED_LIST  0x00000400U
#define PSYQ_DMA_BUSY              0x01000000U
#define PSYQ_DMA_TRIGGER           0x10000000U
#define PSYQ_DMA_MDEC_IN_ENABLE    0x00000008U
#define PSYQ_DMA_MDEC_OUT_ENABLE   0x00000080U
#define PSYQ_DMA_GPU_ENABLE        0x00000800U
#define PSYQ_DMA_OTC_ENABLE        0x08000000U

/* DMA channel count, interrupt-control fields and initial priority. */
enum {
    PSYQ_DMA_CHANNEL_COUNT = 7,
    PSYQ_DMA_IRQ_MASTER_ENABLE = 0x00800000,
    PSYQ_DMA_IRQ_CONTROL_MASK = 0x00ffffff,
    PSYQ_DMA_IRQ_CHANNEL_ENABLE_SHIFT = 16,
    PSYQ_DMA_IRQ_FLAG_SHIFT = 24,
    PSYQ_DMA_DEFAULT_PRIORITY = 0x33333333,
};

#define PSYQ_DMA_IRQ_CHANNEL_MASK 0x0000007fU
#define PSYQ_DMA_IRQ_FLAGS_MASK   0xff000000U
#define PSYQ_DMA_IRQ_MASTER_FLAG  0x80000000U
#define PSYQ_DMA_IRQ_BUS_ERROR    0x00008000U

#endif
