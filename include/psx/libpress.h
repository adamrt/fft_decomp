#ifndef PSX_LIBPRESS_H
#define PSX_LIBPRESS_H

#include "psx/libetc.h"
#include "psx/types.h"

/* Quantization tables and the cached inverse-DCT coefficient matrix. */
typedef struct {
    u8 iq_y[64];
    u8 iq_c[64];
    s16 dct[64];
} DECDCTENV;

typedef char decdctenv_size_must_be_256[(sizeof(DECDCTENV) == 256) ? 1 : -1];

/* environment */
DECDCTENV* DecDCTGetEnv(DECDCTENV* env);
DECDCTENV* DecDCTPutEnv(DECDCTENV* env);
void DecDCTReset(s32 mode);

/* transfer */
s32 DecDCTBufSize(u32* bs);
void DecDCTin(u32* buf, s32 mode);
void DecDCTout(u32* buf, s32 size);

/* synchronization */
s32 DecDCTinCallback(psyq_interrupt_callback_t func);
s32 DecDCTinSync(s32 unused_mode);
s32 DecDCToutCallback(psyq_interrupt_callback_t func);
s32 DecDCToutSync(s32 unused_mode);

/* variable-length decoding */
s32 DecDCTvlc(u32* frame_data, u32* decode_buffer);
s32 DecDCTvlcSize(s32 words);

/* driver internals */

/* Decode output mode and transfer policy. */
enum {
    PSYQ_PRESS_MODE_RGB24 = 0x01,
    PSYQ_PRESS_MODE_SET_BIT15 = 0x02,
};
enum { PSYQ_PRESS_SYNC_POLL_LIMIT = 1048576 };
#define PSYQ_PRESS_DMA_BLOCK_WORDS 32U

/* MDEC command, status and control fields. */
#define PSYQ_MDEC_COMMAND_RGB16     0x08000000U
#define PSYQ_MDEC_COMMAND_SET_BIT15 0x02000000U
#define PSYQ_MDEC_STATUS_BUSY       0x20000000U
#define PSYQ_MDEC_CONTROL_DMA_IN    0x40000000U
#define PSYQ_MDEC_CONTROL_DMA_OUT   0x20000000U
#define PSYQ_MDEC_CONTROL_RESET     0x80000000U

enum { PSYQ_PRESS_VLC_DEFAULT_HALFWORDS = 0x00ffffff };

/* variable-length decoder state */

/* Resume(NULL) restores this handwritten VLC context; offset0x18 is untouched. */
typedef struct {
    u16* input;
    u16* output;
    u32 bit_window;
    s32 consumed_bits;
    s32 quant_scale_shifted;
    s32 component;
    s32 _unused_18;
    s32 predictor_cr;
    s32 predictor_cb;
    s32 predictor_y;
} psyq_press_vlc_state_t;

typedef struct {
    u16 prefix_bits;
    u16 magnitude_bits;
} psyq_press_dc_entry_t;

typedef struct {
    u32 first_code_and_bits;
    u32 extra_codes;
} psyq_press_ac_entry_t;

extern u32 g_psyq_press_vlc_limit_halfwords;
/* The last 32-bit value aliases g_unit_birthday_month_lengths[0..3] in retail. */
extern psyq_press_vlc_state_t g_psyq_press_vlc_state;
extern const psyq_press_dc_entry_t g_psyq_press_dc_luma_table[];
extern const psyq_press_dc_entry_t g_psyq_press_dc_chroma_table[];
extern const psyq_press_ac_entry_t g_psyq_press_ac_primary_table[];
extern const u32 g_psyq_press_ac_secondary_table[];

/* table uploads */

/* Each cached table packet begins with the command that uploads it. */
typedef struct {
    u32 command;
    u8 iq_y[64];
    u8 iq_c[64];
} psyq_press_quant_table_packet_t;

typedef struct {
    u32 command;
    s16 dct[64];
} psyq_press_scale_table_packet_t;

typedef char assert_press_quant_table_packet_size[sizeof(psyq_press_quant_table_packet_t) == 132 ? 1 : -1];
typedef char assert_press_scale_table_packet_size[sizeof(psyq_press_scale_table_packet_t) == 132 ? 1 : -1];

extern psyq_press_quant_table_packet_t g_psyq_press_quant_table;
extern psyq_press_scale_table_packet_t g_psyq_press_scale_table;

/* Hardware register addresses: MDEC command and status, the MDEC-in and
 * MDEC-out DMA channels and the DMA control register. */
extern volatile u32* g_psyq_press_mdec_command;
extern volatile u32* g_psyq_press_mdec_status;
extern volatile u32* g_psyq_press_dma_in_madr;
extern volatile u32* g_psyq_press_dma_in_bcr;
extern volatile u32* g_psyq_press_dma_in_chcr;
extern volatile u32* g_psyq_press_dma_out_madr;
extern volatile u32* g_psyq_press_dma_out_bcr;
extern volatile u32* g_psyq_press_dma_out_chcr;
extern volatile u32* g_psyq_press_dma_dpcr;

/* diagnostics */
extern const char g_psyq_press_reset_bad_option_format[];
extern const char g_psyq_press_in_sync_name[];
extern const char g_psyq_press_out_sync_name[];
extern const char g_psyq_press_timeout_dma_format[];
extern const char g_psyq_press_timeout_fifo_format[];
extern const char g_psyq_press_timeout_format[];

/* transfers */
void MDEC_reset(s32 mode);
void MDEC_in(u32* buf, u32 size);
void MDEC_out(u32* buf, u32 size);
s32 MDEC_in_sync(void);
s32 MDEC_out_sync(void);
s32 timeout(const char* name);

#endif
