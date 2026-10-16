#ifndef OPEN_SCRIPT_H
#define OPEN_SCRIPT_H

/* OPEN.BIN ending script: its text records, opcodes, XA audio and OPNTEX picture sequence. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* fonts */
/* Eight-byte glyph metrics record addressed by OPEN script text. */
typedef struct ending_font_metrics {
    u8 u;
    u8 v;
    u8 flags;
    u8 bearing;
    u8 height;
    u8 width;
    u8 descender;
    u8 advance;
} ending_font_metrics_t;

extern ending_font_metrics_t* g_ending_script_font_metrics;

/* text records */
/* ending_text_record_t.flags. */
typedef enum ending_text_record_flag {
    ENDING_SCRIPT_RECORD_ACTIVE = 0x01, /* the record holds a text being shown */
} ending_text_record_flag_e;

typedef struct ending_text_record {
    u32 flags; /* ending_text_record_flag_e */
    s32 glyph_count;
    s32 skew; /* 0x08: italic skew added to the top edge of each glyph quad */
    s32 x;
    s32 y;
    RECT glyphs[42];
} ending_text_record_t;
typedef char open_script_record_size_must_be_356[(sizeof(ending_text_record_t) == 356) ? 1 : -1];

extern POLY_FT4 g_ending_script_glyph_primitives[2][672];
/* Two horizontal bands, one at the top and one at the bottom of the screen. */
extern POLY_FT4 g_ending_credit_band_primitives[2];
/* One per display buffer. */
extern POLY_FT4 g_ending_fade_primitives[2];

void ending_draw_text_records(u32* otag, s32 graphics_buffer_index);

/* interpreter */
typedef enum ending_script_dispatch_flag {
    ENDING_SCRIPT_DISPATCH_RUNNING = 0x01,  /* cleared to stop the dispatcher and hide the text records */
    ENDING_SCRIPT_DISPATCH_CONTINUE = 0x02, /* runs another entry this frame */
    ENDING_SCRIPT_DISPATCH_WAITING = 0x04,  /* a wait opcode is in progress */
    ENDING_SCRIPT_DISPATCH_XA_PLAYING = 0x08,
    ENDING_SCRIPT_DISPATCH_XA_STOPPING = 0x10, /* the stop sequence of ending_update_xa_audio runs */
    ENDING_SCRIPT_DISPATCH_XA_STARTING = 0x20, /* its start sequence runs */
    ENDING_SCRIPT_DISPATCH_INCREASE_FADE_INTENSITY = 0x40,
    ENDING_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY = 0x80,
} ending_script_dispatch_flag_e;

/* Byte-addressed ending-script cursor. */
typedef struct ending_script_cursor {
    u8* data_base;
    s32 byte_offset;
} ending_script_cursor_t;
typedef char open_script_cursor_size_must_be_8[(sizeof(ending_script_cursor_t) == 8) ? 1 : -1];

/* The interpreter's own state, at the head of g_ending_script_state. */
typedef struct ending_script_dispatch_state {
    /* 0x00 */ u32 flags; /* ending_script_dispatch_flag_e */
    /* 0x04 */ s32 tpage;
    /* 0x08 */ s32 clut;
    /* 0x0c */ s32 font_index; /* each font has 0x380 bytes of glyph metrics */
    /* 0x10 */ s32 skew;       /* copied into each text record's skew */
    /* 0x14 */ s32 wide_glyphs;
    /* 0x18 */ s32 scroll_speed;
    /* 0x1c */ s32 scroll_fraction;
    /* 0x20 */ s32 scrolled_distance;
    /* 0x24 */ s32 wait_counter;
    /* 0x28 */ ending_script_cursor_t cursor;
} ending_script_dispatch_state_t;
typedef char open_script_dispatch_state_size_must_be_48[(sizeof(ending_script_dispatch_state_t) == 48) ? 1 : -1];

/* OPNTEX frame sequence and the script screen fade it starts. */
typedef struct ending_opntex_sequence {
    /* 0x00 */ s32 fade_elapsed_frames;
    /* 0x04 */ s32 fade_duration_frames;
    /* 0x08 */ s32 fade_intensity;
    /* 0x0c */ s32 first_frame;
    /* 0x10 */ s32 last_frame;
    /* 0x14 */ s32 frame_duration;
    /* 0x18 */ s32 frame_countdown;
    /* 0x1c */ s32 delay;
} ending_opntex_sequence_t;

/* The dispatcher state, the 16 text records and the control/OPNTEX tail are
 * one object: ending_update_opntex_sequence reaches dispatch.flags at
 * -0x1698 from its &opntex.frame_duration base. */
typedef struct ending_script_state {
    /* 0x0000 */ ending_script_dispatch_state_t dispatch;
    /* 0x0030 */ ending_text_record_t records[16];
    /* 0x1670 */ s32 _unused_1670;
    /* 0x1674 */ s32 _unknown_1674;     /* cleared by ending_init_script */
    /* 0x1678 */ s32 control_parameter; /* XA channel of ENDING.XA to play */
    /* 0x167c */ s32 control_state;     /* step of the XA start or stop sequence (ending_update_xa_audio) */
    /* 0x1680 */ s32 xa_wait_frames;    /* frames since the XA read started */
    /* 0x1684 */ ending_opntex_sequence_t opntex;
    /* 0x16a4 */ s32 opntex_control_value;
} ending_script_state_t;
typedef char open_script_state_size_must_be_5800[(sizeof(ending_script_state_t) == 5800) ? 1 : -1];

extern ending_script_state_t g_ending_script_state;
/* Scalar views of 32-bit fields of g_ending_script_state. ending_opcode_02_set_scroll_speed,
 * ending_scroll_text and
 * ending_update_opntex_sequence load and store each of these by absolute
 * address; the member spelling lets cse keep the address in a register. */
extern s32 g_ending_script_byte_offset;       /* dispatch.cursor.byte_offset */
extern s32 g_ending_script_scroll_fraction;   /* dispatch.scroll_fraction */
extern s32 g_ending_script_scrolled_distance; /* dispatch.scrolled_distance */
extern u8* g_ending_script_initial_data_base;

/* opcodes */
/* Serialized ending-script opcodes, in the order of g_ending_opcode_handlers. */
typedef enum ending_opcode {
    ENDING_SCRIPT_OPCODE_END = 0x00,
    ENDING_SCRIPT_OPCODE_POSITIONED_TEXT = 0x01,
    ENDING_SCRIPT_OPCODE_SCROLL_SPEED = 0x02,
    ENDING_SCRIPT_OPCODE_REVEAL_SCREEN = 0x03,
    ENDING_SCRIPT_OPCODE_HIDE_SCREEN = 0x04,
    ENDING_SCRIPT_OPCODE_WAIT_FRAMES = 0x05,
    ENDING_SCRIPT_OPCODE_WAIT_SCROLL = 0x06,
    ENDING_SCRIPT_OPCODE_START_XA_AUDIO = 0x07,
    ENDING_SCRIPT_OPCODE_FADE_IN_AUDIO = 0x08,
    ENDING_SCRIPT_OPCODE_FADE_OUT_AUDIO = 0x09,
    ENDING_SCRIPT_OPCODE_STOP_XA_AUDIO = 0x0a,
    ENDING_SCRIPT_OPCODE_LOAD_OPNTEX_FRAME = 0x0b,
    ENDING_SCRIPT_OPCODE_FADE_IN_PICTURE = 0x0c,
    ENDING_SCRIPT_OPCODE_FADE_OUT_PICTURE = 0x0d,
    ENDING_SCRIPT_OPCODE_WAIT_XA_AUDIO_IDLE = 0x0e,
    ENDING_SCRIPT_OPCODE_START_OPNTEX_SEQUENCE = 0x0f,
} ending_opcode_e;

/* The ending-script dispatcher passes a serialized 16-bit cursor. */
typedef void (*ending_opcode_handler_t)(s16* entry);

extern ending_opcode_handler_t g_ending_opcode_handlers[16];

void ending_opcode_00_end_script(s16* unused_entry);
void ending_opcode_01_create_positioned_text_record(s16* command);
void ending_opcode_02_set_scroll_speed(s16* entry);
void ending_opcode_03_start_overlay_fade_out(s16* command);
void ending_opcode_04_start_overlay_fade_in(s16* command);
void ending_opcode_05_wait_counted(s16* entry);
void ending_opcode_06_wait_for_scroll(s16* entry);
void ending_opcode_07_start_xa_audio(s16* command);
void ending_opcode_08_start_sound_fade_in(s16* command);
void ending_opcode_09_start_sound_fade_out(s16* command);
void ending_opcode_0a_stop_xa_audio(s16* unused_entry);
void ending_opcode_0b_load_opntex_frame(s16* command);
void ending_opcode_0c_start_fade_intensity_increase(s16* command);
void ending_opcode_0d_start_fade_intensity_decrease(s16* command);
void ending_opcode_0e_wait_for_xa_audio_idle(s16* unused_entry);
void ending_opcode_0f_start_opntex_sequence(s16* command);

#endif
