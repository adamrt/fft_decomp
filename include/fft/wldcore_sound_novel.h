#ifndef WLDCORE_SOUND_NOVEL_H
#define WLDCORE_SOUND_NOVEL_H

/* WLDCORE's sound novels: the script files, the interpreter and its opcodes, the saved reading states, the text
 * history, waits and the countdown timer. */

#include "fft/main.h"
#include "fft/main_save.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* script files */
/* Section byte offsets at the start of a loaded sound-novel script. */
typedef struct sound_novel_script_file {
    u32 _unused_00;
    u32 offsets[0]; /* 0x04; variable-length section byte-offset table */
} sound_novel_script_file_t;

/* The loaded sound-novel script (sound_novel_load_files). */
extern struct sound_novel_script_file g_sound_novel_script;
/* Sector offsets of each sound novel's script in SNPLBIN.BIN and its text in SNPLMES.BIN. */
extern s32 g_sound_novel_script_sector_offsets[];
extern s32 g_sound_novel_text_sector_offsets[];
void sound_novel_load_files(s32 chapter);

/* interpreter */
/* Interpreter state of the sound-novel script (sound_novel_reading_t.state_flags),
 * stepped by sound_novel_run_until_yield. */
typedef enum sound_novel_state_flag {
    SOUND_NOVEL_STATE_BRANCH = 0x002,  /* a branch was taken: mark its saved-data bit and enter the next section */
    SOUND_NOVEL_STATE_FETCH = 0x004,   /* the handler is done: load the next 32-bit instruction */
    SOUND_NOVEL_STATE_YIELD = 0x008,   /* leave the interpreter until the next frame */
    SOUND_NOVEL_STATE_FETCHED = 0x010, /* the 32-bit instruction was loaded this turn: the handler's first call */
    SOUND_NOVEL_STATE_END = 0x020,     /* the script ended */
    SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE = 0x040,
    SOUND_NOVEL_STATE_VALUE_DISPLAY = 0x080, /* the numeric value display is up */
    SOUND_NOVEL_STATE_WHITE_OVERLAY = 0x100, /* the white full-screen overlay is up */
} sound_novel_state_flag_e;

/* The live sound-novel state from the current instruction on: g_sound_novel_state views
 * g_sound_novel_reading from its offset 0x08. Handlers must reach it as in-struct references: the target
 * keeps each state_flags store ahead of the window and render-record updates and reloads render_index after them,
 * which GCC only does for in-struct accesses. g_sound_novel_state_flags is a separate scalar view for handlers
 * whose flag loads need their own symbol base. */
typedef struct sound_novel_state {
    u32 instruction; /* 0x00 */
    u16 state_flags; /* 0x04; g_sound_novel_state_flags; sound_novel_state_flag_e */
    u8 _unused_06[0x0e];
    s16 render_index; /* 0x14 */
    s16 picture;      /* 0x16 */
    u8 _unused_18[0x0e];
    s16 x; /* 0x26 */
    s16 y; /* 0x28 */
} sound_novel_state_t;

extern sound_novel_state_t g_sound_novel_state;
/* Scalar view of g_sound_novel_reading.state_flags for opcode
 * handlers that read-modify-write it beside another record store: the member
 * spelling shares one base register where the target uses absolute addresses. */
extern u16 g_sound_novel_state_flags;
extern void (*g_sound_novel_opcode_handlers[])(void); /* by the instruction's low byte */
s32 sound_novel_run_until_yield(void);
u32 sound_novel_find_section_instruction(s32 section, s32 opcode);
void sound_novel_opcode_05_load_background_set(void);
void sound_novel_opcode_0a_load_picture(void);
void sound_novel_opcode_1c_start_pending_value_transition(void);
void sound_novel_opcode_1e_apply_counter_delta(void);

/* saved readings */
extern sound_novel_reading_t g_sound_novel_reading;
/* The sound novel being read: it selects the script files, and below 5 its saved reading in
 * g_sound_novel_saved_readings. */
extern s16 g_sound_novel_selected;
void sound_novel_reset_selected_reading(s32 unused);
void sound_novel_restore_reading(s32 render_index);

/* text history */
/* Unsigned views of g_sound_novel_reading.text_history_0..3 for the
 * two history shifters, which load them with lhu; the record keeps s16 for
 * the -1 stores of sound_novel_reset_reading. */
extern s16 g_sound_novel_history_0;
extern u16 g_sound_novel_history_1;
extern u16 g_sound_novel_history_2;
extern u16 g_sound_novel_history_3;

/* sounds, waits and the countdown */
/* Sound resource the sound-novel menu passes to suzuki_sound_append_resource. */
extern struct suzuki_sound_resource* g_sound_novel_default_sound_resource;
extern struct suzuki_sound_resource* g_sound_novel_sound_resource;
extern s32 g_sound_novel_wait_counter;
extern s32 g_sound_novel_wait_frames;
extern s32 g_sound_novel_countdown_frames;
extern s32 g_sound_novel_countdown_section;
extern s32 g_sound_novel_countdown_elapsed;
extern u32 g_sound_novel_local_flags[];
extern u8 g_sound_novel_quit_help_text_ids[];

#endif
