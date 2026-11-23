#ifndef PSX_LIBCD_H
#define PSX_LIBCD_H

#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/libspu.h"

#include "psx/types.h"

#define CdlNop        0x01
#define CdlSetloc     0x02
#define CdlReadN      0x06
#define CdlPause      0x09
#define CdlReset      0x0a
#define CdlDemute     0x0c
#define CdlSetfilter  0x0d
#define CdlSetmode    0x0e
#define CdlGetlocL    0x10
#define CdlSeekL      0x15
#define CdlReadS      0x1b
#define CdlModeSF     0x08
#define CdlModeSize0  0x10
#define CdlModeSize1  0x20
#define CdlModeRT     0x40
#define CdlModeSpeed  0x80
#define CdlModeStream 0x100

#define CdlStatShellOpen 0x10
#define CdlStatRead      0x20
#define CdlStatSeek      0x40
#define CdlStatPlay      0x80

#define CdlDataReady   0x01
#define CdlComplete    0x02
#define CdlAcknowledge 0x03
#define CdlDataEnd     0x04
#define CdlDiskError   0x05

#define btoi(b) ((b) / 16 * 10 + (b) % 16)
#define itob(i) ((i) / 10 * 16 + (i) % 10)

typedef enum {
    PSYQ_CD_SECTORS_PER_SECOND = 75,
    PSYQ_CD_SECONDS_PER_MINUTE = 60,
    PSYQ_CD_LEAD_IN_SECTORS = 150,
} psyq_cd_position_unit_e;

typedef enum {
    StFREE = 0,
    StREWIND = 1,
    StCOMPLETE = 2,
    StBUSY = 3,
    StLOCK = 4,
} psyq_cd_stream_status_e;

/* BCD minute/second/sector position; the encoder leaves track unchanged. */
typedef struct {
    u8 minute;
    u8 second;
    u8 sector;
    u8 track;
} CdlLOC;

/* File/channel subheader filter for CdlSetfilter; pad is reserved. */
typedef struct {
    u8 file;
    u8 chan;
    u16 pad;
} CdlFILTER;

/* Psy-Q CdlATV: CD audio to SPU attenuation (left to left, left to right,
 * right to right, right to left). */
typedef struct {
    u8 val0;
    u8 val1;
    u8 val2;
    u8 val3;
} CdlATV;
typedef char cdl_atv_size_must_be_4[(sizeof(CdlATV) == 4) ? 1 : -1];

/* CD streaming sector header. The linked driver reuses id for the StFREE/... ring state. */
typedef struct {
    u16 id;
    u16 type;
    u16 secCount;
    u16 nSectors;
    u32 frameCount;
    u32 frameSize;
    u16 width;
    u16 height;
    u32 dummy1;
    u32 dummy2;
    CdlLOC loc;
} StHEADER;

/* Event byte and result-buffer contract for command and read callbacks. */
typedef void (*CdlCB)(u8 event, u8* result);

extern s32 CdInit(void);
extern int CdSetDebug(int level);
extern CdlCB CdReadyCallback(CdlCB callback);
extern CdlCB CdReadCallback(CdlCB callback);
extern int CdControl(u8, u8*, u8*);
extern int CdControlF(u8, u8*);
extern int CdSync(int, u8*);
extern int CdRead(int, u32*, int);
extern int CdReadSync(int, u8*);
extern int CdReady(int, u8*);
extern int CdGetSector(void*, int);
extern int CdDataSync(int);

s32 CdMix(CdlATV* vol);
s32 CdControlB(u8 command, u8* parameter, u8* result);
s32 CdRead2(s32 mode);
s32 StGetBackloc(CdlLOC* position);
u32 StGetNext(u32** frame_data, u32** header);
void CdFlush(void);
CdlLOC* CdIntToPos(s32 sector, CdlLOC* position);
s32 CdReset(s32 mode);
void StCdInterrupt(void);
u32 StFreeRing(u32* frame_data);
void StRingStatus(s16* free_sectors, s16* read_sectors);
void StUnSetRing(void);
void StSetRing(u32* buffer, u32 sectors);
void StSetStream(u32 mode, u32 start_frame, u32 end_frame, psyq_interrupt_callback_t frame_callback,
    psyq_interrupt_callback_t end_callback);
void StSetMask(u32 mask, u32 start_frame, u32 end_frame);
s32 CdPosToInt(CdlLOC* position);
char* CdComstr(u8 command);
char* CdIntstr(u8 interrupt);
int CdStatus(void);
s32 CdMode(void);
s32 CdLastCom(void);
CdlLOC* CdLastPos(void);
CdlCB CdSyncCallback(CdlCB callback);

psyq_interrupt_callback_t CdDataCallback(psyq_interrupt_callback_t callback);
void StClearRing(void);

/* driver internals */

typedef enum {
    PSYQ_CD_DATA_WORDS = 2048 / sizeof(u32),
    PSYQ_CD_SECTOR_2340_WORDS = 2340 / sizeof(u32),
    PSYQ_CD_SECTOR_2328_WORDS = 2328 / sizeof(u32),
    PSYQ_CD_READ_TIMEOUT_VBLANKS = 1200,
    PSYQ_CD_SECTOR_TIMEOUT_VBLANKS = 60,
    PSYQ_CD_STREAM_HEADER_WORDS = 8,
    PSYQ_CD_STREAM_PAYLOAD_WORDS = 504,
    PSYQ_CD_STREAM_PAYLOAD_BYTES = PSYQ_CD_STREAM_PAYLOAD_WORDS * sizeof(u32),
    PSYQ_CD_STREAM_HEADER_MAGIC = 0x160,
    PSYQ_CD_DMA_CHANNEL = 3,
} psyq_cd_read_control_e;

/* Resident LIBCD state, reconstructed from the original register accesses. */
/* Driver status uses 32-bit storage; public status queries read its low byte. */
extern u32 CD_status;
extern u8 CD_mode;
extern u8 CD_com;
extern CdlLOC CD_pos;

extern char* CD_comstr[28];
extern char* CD_intstr[7];
extern char g_psyq_cd_unknown_name[];
extern s32 CD_debug;
extern CdlCB CD_cbsync;
extern CdlCB CD_cbready;
extern CdlCB g_psyq_cd_read_callback;
void def_cbsync(u8 unused_event, u8* unused_result);
void def_cbready(u8 unused_event, u8* unused_result);
void def_cbread(u8 unused_event, u8* unused_result);
void CD_flush(void);
s32 CD_vol(CdlATV* volume);
s32 CD_getsector(void* destination, s32 words);
s32 CD_datasync(s32 mode);
s32 CD_sync(s32 mode, u8* result);
s32 CD_ready(s32 mode, u8* result);
void psyq_cd_stream_ready_callback(u8 unused_event, u8* unused_result);

extern s32 g_psyq_cd_stream_ring_sectors;
extern char g_psyq_cd_init_failure_message[];
void CD_initintr(void);
s32 CD_init(void);
s32 CD_initvol(void);
void CD_set_test_parmnum(s32 parameter_count);

/* command and ring helpers */
s32 CD_cw(u8 command, const u8* parameter, u8* result, s32 asynchronous);
void callback(void);
s32 init_ring_status(u32 first_sector, u32 sector_count);
void mem2mem(u32* destination, u32* source, u32 words, s32 unused_frame_complete);

/* Original data pointers (typed volatile at hardware register accesses). */
extern volatile u8* g_psyq_cd_index_status_register;
extern volatile u8* g_psyq_cd_command_response_register;
extern volatile u8* g_psyq_cd_parameter_data_register;
extern volatile u8* g_psyq_cd_request_interrupt_register;
extern volatile u32* g_psyq_cd_common_delay_register;
extern volatile psyq_spu_registers_t* g_psyq_cd_spu_registers;
extern volatile u8* g_psyq_cd_unset_index_register;
extern volatile u8* g_psyq_cd_unset_request_register;
extern u32 CD_status1;
extern char g_psyq_cd_init_message[];
extern char g_psyq_cd_init_address_format[];

extern StHEADER* g_psyq_cd_stream_ring;
extern u32 g_psyq_cd_stream_write_index;
extern u32 g_psyq_cd_stream_frame_start_index;
extern u32 g_psyq_cd_stream_read_index;
extern s32 g_psyq_cd_stream_emulation_base;
extern u32 g_psyq_cd_stream_end_frame;
extern u32 g_psyq_cd_stream_start_mask;
extern s16 g_psyq_cd_stream_expected_sector_index;
extern u32 g_psyq_cd_stream_frame_count;
extern u32 g_psyq_cd_stream_mode;
extern s32 g_psyq_cd_stream_skip_sector_position;
extern void (*g_psyq_cd_stream_frame_callback)(void);
extern void (*g_psyq_cd_stream_end_callback)(void);
extern s32 g_psyq_cd_stream_interrupt_pending;
extern s32 g_psyq_cd_stream_requested_channel;
extern u32 g_psyq_cd_stream_start_frame;
extern s32 g_psyq_cd_stream_current_channel;
extern s32 g_psyq_cd_stream_frame_dma_pending;
extern CdlLOC g_psyq_cd_backloc_position;
extern u32 g_psyq_cd_backloc_frame;

/* Remaining low-level CD register aliases and command table. */
extern s32 g_psyq_cd_command_uses_position[32];
extern volatile u32* g_psyq_cd_access_delay_register; /* CD access delay: 1f801018 */
extern volatile u32* g_psyq_cd_dma_priority_register; /* DMA priority: 1f8010f0 */
extern volatile u32* g_psyq_cd_dma_address_register;  /* DMA3 MADR */
extern volatile u32* g_psyq_cd_dma_block_register;    /* DMA3 BCR */
extern volatile u32* g_psyq_cd_dma_control_register;  /* DMA3 CHCR */
s32 getintr(void);
extern u8 g_psyq_cd_sync_result[8];
extern u8 g_psyq_cd_ready_result[8];
extern char g_psyq_cd_timeout_message[];
extern char g_psyq_cd_timeout_format[];
extern char g_psyq_cd_data_sync_timeout_name[];
void data_ready_callback(void);
extern u8* g_psyq_cd_init_irq_state_pointer;

/* CD command attribute tables (remain original data). */
enum { PSYQ_CD_COMMAND_TEST = 0x19 };
typedef struct {
    s32 waits_for_complete[32];
    s32 resets_ready[32];
    s32 ack_updates_status[32];
    s32 parameter_counts[32];
} psyq_cd_command_attributes_t;
extern psyq_cd_command_attributes_t g_psyq_cd_command_attributes;
extern s32 CD_nopen;
extern u8 g_psyq_cd_second_ready_result[8];
extern char g_psyq_cd_disk_error_message[];
extern char g_psyq_cd_disk_error_format[];
extern char g_psyq_cd_unknown_interrupt_message[];
extern char g_psyq_cd_unknown_interrupt_format[];
extern char g_psyq_cd_sync_timeout_name[];
extern char g_psyq_cd_ready_timeout_name[];
extern char g_psyq_cd_command_trace_format[];
extern char g_psyq_cd_missing_parameters_format[];
extern char g_psyq_cd_command_timeout_name[];

typedef struct {
    u8 sync;
    u8 ready;
    u8 second_ready;
} psyq_cd_interrupt_result_t;
extern volatile psyq_cd_interrupt_result_t g_psyq_cd_irq_state;
/* The ready byte also has a measured store/reload in getintr. */

typedef struct {
    s32 sectors;
    u32* first_buffer;
    u32* next_buffer;
    s32 mode;
    s32 words;
    s32 remaining;
    s32 sector_clock;
    s32 start_clock;
    s32 expected_sector;
    CdlCB sync_callback;
    CdlCB ready_callback;
} psyq_cd_read_state_t;
extern volatile psyq_cd_read_state_t g_psyq_cd_read_state;

void cb_read(u8 event, u8* result);
s32 cd_read_retry(s32 retry);
extern char g_psyq_cd_sector_error_message[];
extern char g_psyq_cd_shell_open_message[];
extern char g_psyq_cd_read_retry_message[];

/* Streaming controller aliases, bound to original 32-bit pointer slots. */
extern volatile u8* g_psyq_cd_stream_index_register;
extern volatile u8* g_psyq_cd_stream_data_register;
extern volatile u8* g_psyq_cd_stream_request_register;
extern volatile u32* g_psyq_cd_stream_access_delay_register;
extern volatile u32* g_psyq_cd_stream_common_delay_register;
extern volatile u32* g_psyq_cd_stream_dma_priority_register;
typedef union {
    u32 word;
    u8 bytes[4];
} psyq_cd_dma_interrupt_register_t;
extern volatile psyq_cd_dma_interrupt_register_t* g_psyq_cd_stream_dma_interrupt_register;
extern volatile u32* g_psyq_cd_stream_mdec_out_dma_control_register;
extern volatile u32* g_psyq_cd_stream_dma_control_register;
extern s32 g_psyq_cd_stream_diagnostic_code;
extern u32 g_psyq_cd_stream_emulation_sector_index;
extern u32* g_psyq_cd_stream_payload;
extern StHEADER* g_psyq_cd_stream_current_header;
extern char g_psyq_cd_dma_timeout_format[];
void dma_execute(s32 channel, u32* destination, u32 blocks, u32 words, u32 control, u8 enable_irq, s32 unused);

/* LIBCD wait timeout state, 12 bytes at 0x80032a94. */
typedef struct {
    s32 deadline;
    s32 counter;
    char* description;
} psyq_cd_timeout_state_t;
extern psyq_cd_timeout_state_t g_psyq_cd_timeout;

/* Original completed-location/frame record, eight bytes at 0x80032aa0.
 * Position is copied by StGetBackloc; its adjacent 32-bit frame field is returned.
 * Keep the old position/frame aliases for already-matched fresh-address loads.
 */
typedef struct {
    CdlLOC position;
    u32 frame;
} psyq_cd_backloc_state_t;
extern psyq_cd_backloc_state_t g_psyq_cd_backloc;

/* shared driver helpers */
static __inline__ s32 psyq_cd_issue_with_retry(u8 command, const u8* parameter, u8* result, s32 asynchronous) {
    CdlCB callback = CD_cbsync;
    s32 attempts = 3;
    do {
        CD_cbsync = 0;
        if (command != CdlNop) {
            u8 status = CD_status;
            if (status & CdlStatShellOpen)
                CD_cw(CdlNop, 0, 0, 0);
        }
        if (!parameter || !g_psyq_cd_command_uses_position[command] || CD_cw(CdlSetloc, parameter, result, 0) == 0) {
            CD_cbsync = callback;
            if (CD_cw(command, parameter, result, asynchronous) == 0)
                return 0;
        }
    } while (--attempts != -1);
    CD_cbsync = callback;
    return -1;
}

static __inline__ void psyq_cd_set_timeout(char* description) {
    g_psyq_cd_timeout.deadline = VSync(-1) + 960;
    g_psyq_cd_timeout.counter = 0;
    g_psyq_cd_timeout.description = description;
}

static __inline__ s32 psyq_cd_check_timeout(void) {
    if (g_psyq_cd_timeout.deadline < VSync(-1) || g_psyq_cd_timeout.counter++ > 0x3c0000) {
        puts(g_psyq_cd_timeout_message);
        printf(g_psyq_cd_timeout_format, g_psyq_cd_timeout.description, CD_comstr[CD_com],
            CD_intstr[g_psyq_cd_irq_state.sync], CD_intstr[g_psyq_cd_irq_state.ready]);
        CD_flush();
        return -1;
    }
    return 0;
}

static __inline__ void psyq_cd_apply_volume(const CdlATV* volume) {
    *g_psyq_cd_index_status_register = 2;
    *g_psyq_cd_parameter_data_register = volume->val0;
    *g_psyq_cd_request_interrupt_register = volume->val1;
    *g_psyq_cd_index_status_register = 3;
    *g_psyq_cd_command_response_register = volume->val2;
    *g_psyq_cd_parameter_data_register = volume->val3;
    *g_psyq_cd_request_interrupt_register = 0x20;
}

static __inline__ void psyq_cd_copy_result(u8* destination, const u8* source, u32 size) {
    u8* output = destination;
    const u8* input = source;
    if (!output)
        return;
    while (size--)
        *output++ = *input++;
}

static __inline__ void psyq_cd_dispatch_callbacks(void) {
    s32 events;
    u8 bank = *g_psyq_cd_index_status_register & 3;
    while ((events = getintr()) != 0) {
        if ((events & 4) && CD_cbready)
            CD_cbready(g_psyq_cd_irq_state.ready, g_psyq_cd_ready_result);
        if ((events & 2) && CD_cbsync)
            CD_cbsync(g_psyq_cd_irq_state.sync, g_psyq_cd_sync_result);
    }
    *g_psyq_cd_index_status_register = bank;
}

#endif
