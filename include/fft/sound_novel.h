#ifndef SOUND_NOVEL_H
#define SOUND_NOVEL_H

/* The Sound Novels: reading, the history, quit and restart, and the sound novel script machine with its opcode
 * handlers. */

#include "fft/menu_stack.h"
#include "psx/types.h"

struct suzuki_sound_resource;

/* readings */
struct sound_novel_script_file;

/* A saved sound-novel reading state (g_sound_novel_saved_readings, 0x38 bytes each).
 * sound_novel_reset_selected_reading establishes the stride and
 * sound_novel_reset_reading resets fields selectively; meanings beyond
 * those stores remain provisional. */
typedef struct sound_novel_reading {
    /* 0x00: loaded sound-novel script, including its section-offset table. */
    struct sound_novel_script_file* buffer;
    u32* script_words;        /* 0x04 */
    u32 instruction;          /* 0x08 */
    u16 state_flags;          /* 0x0c; sound_novel_state_flag_e */
    s16 section;              /* 0x0e */
    s16 word_index;           /* 0x10; index of the 32-bit instruction in the section; cleared by
                               * sound_novel_restore_reading
                               */
    s16 counter_delta;        /* 0x12 */
    s16 text_id;              /* 0x14 */
    s16 brightness;           /* 0x16 */
    s16 _unknown_18;          /* 0x18 */
    s16 background_set;       /* 0x1a */
    s16 picture_render_index; /* 0x1c */
    s16 picture_id;           /* 0x1e */
    s16 text_history[4];      /* 0x20; oldest to newest */
    s16 value_display_mode;   /* 0x28 */
    s16 music_selection;      /* 0x2a */
    s16 sound_mode;           /* 0x2c; 0 plays pending sounds only, 1 also switches the music track */
    s16 picture_x;            /* 0x2e */
    s16 picture_y;            /* 0x30 */
    s16 sound_id;             /* 0x32 */
    s32 counter;              /* 0x34 */
} sound_novel_reading_t;
typedef char sound_novel_reading_size_must_be_56[sizeof(sound_novel_reading_t) == 56 ? 1 : -1];

extern sound_novel_reading_t g_sound_novel_reading;
extern sound_novel_reading_t g_sound_novel_saved_readings[5];
extern u32 g_sound_novel_read_section_bits[40];
extern u32 g_sound_novel_saved_flags[2];

/* The sound novel being read: it selects the script files, and below 5 its saved reading in
 * g_sound_novel_saved_readings. */
extern s16 g_sound_novel_selected;

void sound_novel_reset_selected_reading(s32 unused);
void sound_novel_restore_reading(s32 render_index);

/* script files */
/* Section byte offsets at the start of a loaded sound-novel script. */
typedef struct sound_novel_script_file {
    u32 _unused_00;
    u32 offsets[0]; /* 0x04; variable-length section byte-offset table */
} sound_novel_script_file_t;

/* The loaded sound-novel script (sound_novel_load_files). */
extern struct sound_novel_script_file g_sound_novel_script;

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

s32 sound_novel_run_until_yield(void);
u32 sound_novel_find_section_instruction(s32 section, s32 opcode);
void sound_novel_opcode_05_load_background_set(void);
void sound_novel_opcode_0a_load_picture(void);
void sound_novel_opcode_1c_start_pending_value_transition(void);
void sound_novel_opcode_1e_apply_counter_delta(void);

/* text history */
/* Unsigned scalar view of text_history[1] used by both history shifters.
 * Direct member access changes their original instruction scheduling. */
extern u16 g_sound_novel_history_1;

/* sounds, waits and the countdown */
extern struct suzuki_sound_resource* g_sound_novel_sound_resource;
extern s32 g_sound_novel_countdown_frames;

/* menu levels */
/* Values of sound_novel_level_t.phase. */
typedef enum sound_novel_phase {
    SOUND_NOVEL_PHASE_RUN = 0,          /* the script runs */
    SOUND_NOVEL_PHASE_OPEN_HISTORY = 2, /* square: counts down, then pushes the history level */
    SOUND_NOVEL_PHASE_END = 3,          /* the script ended: waits for thread 14, then leaves */
} sound_novel_phase_e;

/* The SOUND_NOVEL level pushed by sound_novel_push_level and stepped by
 * sound_novel_step_level while the script runs: the Start and Square button prompts, and the
 * brightness bar with its overlay, shown for a while after L1 or R1 changes the brightness. */
typedef struct sound_novel_level {
    s32 quit_prompt_window;        /* 0x00; the Start button prompt (the Quit/Continue menu) */
    s32 history_prompt_window;     /* 0x04; the Square button prompt (the text history) */
    s32 brightness_image;          /* 0x08; the brightness bar, a VRAM strip render record */
    s32 brightness_overlay_window; /* 0x0c; the brightness bar's overlay */
    u8 _unused_10[0xc];
    s32 treasure_id; /* 0x1c; the treasure detail reopened when the novel ends */
    s32 _unknown_20; /* 0x20; set to 1 at push */
    s32 countdown;   /* 0x24; frames before the text history opens (9999 at push) */
    s32 phase;       /* 0x28; sound_novel_phase_e */
    s32 fade_timer;  /* 0x2c; frames the brightness bar stays up (60 after L1 or R1) */
} sound_novel_level_t;

/* The SOUND_NOVEL_QUIT level, the sound novel's Quit/Continue menu; pushed by
 * sound_novel_push_quit_level and stepped by sound_novel_step_quit_level. */
typedef struct sound_novel_quit_level {
    s32 cursor_window; /* 0x00 */
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 _unused_0c;
    s32 fade_out;    /* 0x10 */
    s32 fade_in;     /* 0x14 */
    s32 _unknown_18; /* 0x18; cleared at push */
    s32 choice;      /* 0x1c */
} sound_novel_quit_level_t;

/* Values of sound_novel_history_level_t.phase. */
typedef enum sound_novel_history_phase {
    SOUND_NOVEL_HISTORY_PHASE_BROWSE = 0,
    SOUND_NOVEL_HISTORY_PHASE_DIM = 1,     /* five steps: the picture darkens under the fill box */
    SOUND_NOVEL_HISTORY_PHASE_RESTORE = 2, /* the reverse ramp, then the level pops */
} sound_novel_history_phase_e;

/* Provisional: the SOUND_NOVEL_HISTORY level pushed by sound_novel_push_history_level, which stores
 * the newest history text, the three older ones, phase DIM and step 0. The draw helper reads the same record. */
typedef struct sound_novel_history_level {
    s32 upper_window;   /* 0x00 */
    s32 lower_window;   /* 0x04 */
    s32 render_index;   /* 0x08 */
    s32 newest_text_id; /* 0x0c; newest history text */
    s32 text_ids[3];    /* 0x10 */
    s32 scroll;         /* 0x1c */
    s32 phase;          /* 0x20; sound_novel_history_phase_e */
    s32 step;           /* 0x24; 0..4 fade step */
} sound_novel_history_level_t;

/* The SOUND_NOVEL_RESTART level pushed by sound_novel_push_restart_level and stepped by
 * sound_novel_step_restart_level: a cursor window over the saved-picture render record and two text
 * render records. Only mode shares its meaning with the EVENT_TRANSITION level at the same offset. */
typedef struct sound_novel_restart_level {
    s32 cursor_window;  /* 0x00; moved between the two option rows */
    s32 picture_render; /* 0x04; VRAM render record of the TIM at g_picture_buffer */
    s32 message_render; /* 0x08; TEXT_ID_SUBSTITUTED_TEXT */
    s32 option_render;  /* 0x0c; TEXT_ID_SOUND_NOVEL_RESUME_CHOICES; the cursor rows are relative to its base */
    s32 dissolve_step;  /* 0x10; step of the progressive picture upload, 0 once it finishes */
    s32 cursor_row;     /* 0x14; 0 or 1, toggled by up/down; 1 resets the selected saved record */
    u8 _unused_18[8];
    s32 mode; /* 0x20; argument of the push */
} sound_novel_restart_level_t;

void sound_novel_resume_level(menu_stack_record_t* level);
void sound_novel_step_level(menu_stack_record_t* level);
void sound_novel_suspend_level(menu_stack_record_t* level);
void sound_novel_resume_quit_level(menu_stack_record_t* level);
void sound_novel_suspend_quit_level(menu_stack_record_t* level);
void sound_novel_step_quit_level(menu_stack_record_t* level);
void sound_novel_step_history_level(menu_stack_record_t* level);
void sound_novel_step_restart_level(menu_stack_record_t* level);

#endif
