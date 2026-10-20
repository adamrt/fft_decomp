#ifndef ENDING_H
#define ENDING_H

/* OPEN's ending: the sequence that plays it and the scrolling ending script, with its fonts, text records, interpreter
 * state, XA audio, OPNTEX picture sequence and opcode handlers. */

#include "fft/controller.h"
#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* ending sequence */
/* Run OPEN's ending movie and script, then restore graphics state. */
void open_ending_entrypoint(void);
void ending_init(void);
void ending_graphics_load(void);

/* text records */
/* ending_text_record_t.flags. */
typedef enum ending_text_record_flag {
    ENDING_SCRIPT_RECORD_ACTIVE = 0x01 /* the record holds a text being shown */
} ending_text_record_flag_e;

typedef struct ending_text_record {
    u32 flags; /* ending_text_record_flag_e */
    s32 glyph_count;
    s32 skew; /* 0x08: italic skew added to the top edge of each glyph quad */
    point32_t position;
    RECT glyphs[42];
} ending_text_record_t;
typedef char ending_text_record_size_must_be_356[(sizeof(ending_text_record_t) == 356) ? 1 : -1];

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
    ENDING_SCRIPT_DISPATCH_DECREASE_FADE_INTENSITY = 0x80
} ending_script_dispatch_flag_e;

/* Byte-addressed ending-script cursor. */
typedef struct ending_script_cursor {
    u8* data_base;
    s32 byte_offset;
} ending_script_cursor_t;
typedef char ending_script_cursor_size_must_be_8[(sizeof(ending_script_cursor_t) == 8) ? 1 : -1];

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
typedef char ending_script_dispatch_state_size_must_be_48[(sizeof(ending_script_dispatch_state_t) == 48) ? 1 : -1];

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
typedef char ending_script_state_size_must_be_5800[(sizeof(ending_script_state_t) == 5800) ? 1 : -1];

extern ending_script_state_t g_ending_script_state;

/* The ending script's controller handler (CONTROLLER_SCRIPT). */
void ending_update_script(controller_record_t* unused_record);

#endif
