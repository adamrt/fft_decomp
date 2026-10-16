#ifndef OPEN_SCRIPT_H
#define OPEN_SCRIPT_H

/* OPEN.BIN ending script: its text records, opcodes, XA audio and OPNTEX picture sequence. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* fonts */
/* Eight-byte glyph metrics record addressed by OPEN script text. */
typedef struct open_font_metrics {
    u8 u;
    u8 v;
    u8 flags;
    u8 bearing;
    u8 height;
    u8 width;
    u8 descender;
    u8 advance;
} open_font_metrics_t;

extern open_font_metrics_t* g_open_script_font_metrics;

/* text records */
/* open_script_record_t.flags. */
typedef enum open_script_record_flag {
    OPEN_SCRIPT_RECORD_ACTIVE = 0x01, /* the record holds a text being shown */
} open_script_record_flag_e;

typedef struct open_script_record {
    u32 flags; /* open_script_record_flag_e */
    s32 glyph_count;
    s32 skew; /* 0x08: italic skew added to the top edge of each glyph quad */
    s32 x;
    s32 y;
    RECT glyphs[42];
} open_script_record_t;
typedef char open_script_record_size_must_be_356[(sizeof(open_script_record_t) == 356) ? 1 : -1];

extern POLY_FT4 g_open_script_glyph_primitives[2][672];
/* Two horizontal bands, one at the top and one at the bottom of the screen. */
extern POLY_FT4 g_open_gfx_credit_band_primitives[2];
/* One per display buffer. */
extern POLY_FT4 g_open_gfx_fade_primitives[2];

void open_script_draw_text_records(u32* otag, s32 graphics_buffer_index);

/* interpreter */
typedef enum open_script_dispatch_flag {
    OPEN_SCRIPT_DISPATCH_RUNNING = 0x01,  /* cleared to stop the dispatcher and hide the text records */
    OPEN_SCRIPT_DISPATCH_CONTINUE = 0x02, /* runs another entry this frame */
    OPEN_SCRIPT_DISPATCH_WAITING = 0x04,  /* a wait opcode is in progress */
    OPEN_SCRIPT_DISPATCH_XA_PLAYING = 0x08,
    OPEN_SCRIPT_DISPATCH_XA_STOPPING = 0x10, /* the stop sequence of open_script_update_xa_audio runs */
    OPEN_SCRIPT_DISPATCH_XA_STARTING = 0x20, /* its start sequence runs */
    OPEN_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY = 0x40,
    OPEN_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY = 0x80,
} open_script_dispatch_flag_e;

/* Byte-addressed ending-script cursor. */
typedef struct open_script_cursor {
    u8* data_base;
    s32 byte_offset;
} open_script_cursor_t;
typedef char open_script_cursor_size_must_be_8[(sizeof(open_script_cursor_t) == 8) ? 1 : -1];

/* The interpreter's own state, at the head of g_open_script_state. */
typedef struct open_script_dispatch_state {
    /* 0x00 */ u32 flags; /* open_script_dispatch_flag_e */
    /* 0x04 */ s32 tpage;
    /* 0x08 */ s32 clut;
    /* 0x0c */ s32 font_index; /* each font has 0x380 bytes of glyph metrics */
    /* 0x10 */ s32 skew;       /* copied into each text record's skew */
    /* 0x14 */ s32 wide_glyphs;
    /* 0x18 */ s32 timing_step;
    /* 0x1c */ s32 timing_fraction;
    /* 0x20 */ s32 external_counter;
    /* 0x24 */ s32 wait_counter;
    /* 0x28 */ open_script_cursor_t cursor;
} open_script_dispatch_state_t;
typedef char open_script_dispatch_state_size_must_be_48[(sizeof(open_script_dispatch_state_t) == 48) ? 1 : -1];

/* OPNTEX frame sequence and the script screen fade it starts. */
typedef struct open_opntex_sequence_state {
    /* 0x00 */ s32 fade_elapsed_frames;
    /* 0x04 */ s32 fade_duration_frames;
    /* 0x08 */ s32 fade_intensity;
    /* 0x0c */ s32 first_frame;
    /* 0x10 */ s32 last_frame;
    /* 0x14 */ s32 frame_duration;
    /* 0x18 */ s32 frame_countdown;
    /* 0x1c */ s32 delay;
} open_opntex_sequence_state_t;

/* The dispatcher state, the 16 text records and the control/OPNTEX tail are
 * one object: open_gfx_update_opntex_sequence reaches dispatch.flags at
 * -0x1698 from its &opntex.frame_duration base. */
typedef struct open_script_state {
    /* 0x0000 */ open_script_dispatch_state_t dispatch;
    /* 0x0030 */ open_script_record_t records[16];
    /* 0x1670 */ s32 _unused_1670;
    /* 0x1674 */ s32 _unknown_1674;     /* cleared by open_script_init_state */
    /* 0x1678 */ s32 control_parameter; /* XA channel of ENDING.XA to play */
    /* 0x167c */ s32 control_state;     /* step of the XA start or stop sequence (open_script_update_xa_audio) */
    /* 0x1680 */ s32 xa_wait_frames;    /* frames since the XA read started */
    /* 0x1684 */ open_opntex_sequence_state_t opntex;
    /* 0x16a4 */ s32 opntex_control_value;
} open_script_state_t;
typedef char open_script_state_size_must_be_5800[(sizeof(open_script_state_t) == 5800) ? 1 : -1];

extern open_script_state_t g_open_script_state;
/* Scalar views of 32-bit fields of g_open_script_state. open_opcode_set_timing_step,
 * open_script_update_timing_and_record_values and
 * open_gfx_update_opntex_sequence load and store each of these by absolute
 * address; the member spelling lets cse keep the address in a register. */
extern s32 g_open_script_byte_offset;      /* dispatch.cursor.byte_offset */
extern s32 g_open_script_timing_fraction;  /* dispatch.timing_fraction */
extern s32 g_open_script_external_counter; /* dispatch.external_counter */
extern u8* g_open_script_initial_data_base;

/* opcodes */
/* Serialized ending-script opcodes, in the order of g_open_opcode_handlers. */
typedef enum open_script_opcode {
    OPEN_SCRIPT_OPCODE_END = 0x00,
    OPEN_SCRIPT_OPCODE_POSITIONED_TEXT = 0x01,
    OPEN_SCRIPT_OPCODE_SCROLL_SPEED = 0x02,
    OPEN_SCRIPT_OPCODE_REVEAL_SCREEN = 0x03,
    OPEN_SCRIPT_OPCODE_HIDE_SCREEN = 0x04,
    OPEN_SCRIPT_OPCODE_WAIT_FRAMES = 0x05,
    OPEN_SCRIPT_OPCODE_WAIT_SCROLL = 0x06,
    OPEN_SCRIPT_OPCODE_START_XA_AUDIO = 0x07,
    OPEN_SCRIPT_OPCODE_FADE_IN_AUDIO = 0x08,
    OPEN_SCRIPT_OPCODE_FADE_OUT_AUDIO = 0x09,
    OPEN_SCRIPT_OPCODE_STOP_XA_AUDIO = 0x0a,
    OPEN_SCRIPT_OPCODE_LOAD_OPNTEX_FRAME = 0x0b,
    OPEN_SCRIPT_OPCODE_FADE_IN_PICTURE = 0x0c,
    OPEN_SCRIPT_OPCODE_FADE_OUT_PICTURE = 0x0d,
    OPEN_SCRIPT_OPCODE_WAIT_XA_AUDIO_IDLE = 0x0e,
    OPEN_SCRIPT_OPCODE_START_OPNTEX_SEQUENCE = 0x0f,
} open_script_opcode_e;

/* The ending-script dispatcher passes a serialized 16-bit cursor. */
typedef void (*open_script_entry_handler_t)(s16* entry);

extern open_script_entry_handler_t g_open_opcode_handlers[16];

void open_opcode_end_script(s16* unused_entry);
void open_opcode_create_positioned_text_record(s16* command);
void open_opcode_set_timing_step(s16* entry);
void open_opcode_start_overlay_fade_out(s16* command);
void open_opcode_start_overlay_fade_in(s16* command);
void open_opcode_wait_counted(s16* entry);
void open_opcode_wait_for_external_counter(s16* entry);
void open_opcode_start_xa_audio(s16* command);
void open_opcode_start_sound_fade_in(s16* command);
void open_opcode_start_sound_fade_out(s16* command);
void open_opcode_stop_xa_audio(s16* unused_entry);
void open_opcode_load_opntex_frame(s16* command);
void open_opcode_start_fade_intensity_increase(s16* command);
void open_opcode_start_fade_intensity_decrease(s16* command);
void open_opcode_wait_for_xa_audio_idle(s16* unused_entry);
void open_opcode_start_opntex_sequence(s16* command);

#endif
