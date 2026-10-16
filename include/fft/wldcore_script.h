#ifndef WLDCORE_SCRIPT_H
#define WLDCORE_SCRIPT_H

/* WLDCORE's map script and sound novel: opcodes, script state, sound requests and text. */

#include "fft/gfx.h"
#include "fft/wldcore.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Interpreter state of the sound-novel script (wldcore_saved_record_t.state_flags),
 * stepped by wldcore_script_run_until_yield. */
typedef enum wldcore_opcode_state_flag {
    WLDCORE_OPCODE_STATE_BRANCH = 0x002,  /* a branch was taken: mark its saved-data bit and enter the next section */
    WLDCORE_OPCODE_STATE_FETCH = 0x004,   /* the handler is done: load the next 32-bit instruction */
    WLDCORE_OPCODE_STATE_YIELD = 0x008,   /* leave the interpreter until the next frame */
    WLDCORE_OPCODE_STATE_FETCHED = 0x010, /* the 32-bit instruction was loaded this turn: the handler's first call */
    WLDCORE_OPCODE_STATE_END = 0x020,     /* the script ended */
    WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE = 0x040,
    WLDCORE_OPCODE_STATE_VALUE_DISPLAY = 0x080, /* the numeric value display is up */
    WLDCORE_OPCODE_STATE_WHITE_OVERLAY = 0x100, /* the white full-screen overlay is up */
} wldcore_opcode_state_flag_e;

/* The world-script state from the 32-bit instruction at 0x800d4848 onwards
 * (the tail of wldcore_saved_record_t). Handlers must reach it as
 * in-struct references: the target keeps each state_flags store ahead of the
 * window/render-record updates and reloads render_index after them, which GCC
 * only does for in-struct accesses. The instruction is an encoded 32-bit value;
 * g_wldcore_state_flags remains a separate scalar view for handlers whose
 * flag loads need their own symbol base. */
typedef struct wldcore_opcode_state {
    u32 instruction; /* 0x00 */
    u16 state_flags; /* 0x04; g_wldcore_state_flags; wldcore_opcode_state_flag_e */
    u8 _unused_06[0x0e];
    s16 render_index; /* 0x14 */
    s16 picture;      /* 0x16 */
    u8 _unused_18[0x0e];
    s16 x; /* 0x26 */
    s16 y; /* 0x28 */
} wldcore_opcode_state_t;

typedef enum wldcore_script_request_flags {
    WLDCORE_SCRIPT_REQUEST_CHOICE = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_CHOICE,
    WLDCORE_SCRIPT_REQUEST_LOAD_EVENT = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_LOAD_EVENT,
    WLDCORE_SCRIPT_REQUEST_TEXT = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_TEXT,
    WLDCORE_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY
    = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY,
    WLDCORE_SCRIPT_REQUEST_SET_VAR_2 = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_SET_VAR_2,
    WLDCORE_SCRIPT_REQUEST_DRAW_PATH = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_DRAW_PATH,
    WLDCORE_SCRIPT_REQUEST_ERASE_PATH = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_ERASE_PATH,
    WLDCORE_SCRIPT_REQUEST_FOCUS_LOCATION = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION,
    WLDCORE_SCRIPT_REQUEST_DRAW_LOCATION = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_DRAW_LOCATION,
    WLDCORE_SCRIPT_REQUEST_ERASE_LOCATION = WLDCORE_SCRIPT_RESULT_ACTION_REACHED | WLDCORE_SCRIPT_ACTION_ERASE_LOCATION,
} wldcore_script_request_flags_e;

/* WLDCORE script interpreter state; the opcode handlers read ip/data here.
 * One object through the choice/argument 32-bit values:
 * wldcore_opcode_map_request_choice reaches ip at -4 and the choice
 * values at +0x10 from its &choice_vars base. */
typedef struct wldcore_script_state {
    s32 flags;          /* 0x00; wldcore_script_result_flags_e | wldcore_script_action_flags_e */
    u16* list;          /* 0x04; script list for the current location */
    u16* data;          /* 0x08 */
    u16 ip;             /* 0x0c */
    s32 choice_vars[4]; /* 0x10; staged Choice variables */
    /* 0x20: script request arguments; the staged Choice values share them. */
    s32 args[4];
} wldcore_script_state_t;
typedef char wldcore_script_state_size_must_be_48[sizeof(wldcore_script_state_t) == 48 ? 1 : -1];

/* wldcore_text_scrollable_window_t.scroll_state. */
typedef enum wldcore_text_scroll_state {
    WLDCORE_TEXT_SCROLL_PAGING = 0,
    WLDCORE_TEXT_SCROLL_MANUAL = 1, /* while square is held: up and down scroll by rows */
    WLDCORE_TEXT_SCROLL_RETURN = 2, /* scrolls back to the page start */
} wldcore_text_scroll_state_e;

/* The text ids of TEXT_ID_WORLD_MENU_BASE that the code names: the bar, the
 * Brave Story, the tutorial and the location menu. The first five entries are
 * the bar's rows, TEXT_ID_WORLD_MENU_BASE + wldcore_bar_menu_row_e. */
typedef enum wldcore_text_id {
    WLDCORE_TEXT_ID_OK_NO = 0xb805,                 /* "O K / N O" */
    WLDCORE_TEXT_ID_BAR_WELCOME = 0xb806,           /* the bar owner: "Welcome. Come in and relax." */
    WLDCORE_TEXT_ID_BAR_GOODBYE = 0xb807,           /* "Thank you! Come again." */
    WLDCORE_TEXT_ID_RUMORS_INTRO = 0xb808,          /* "Rumors are floating around." */
    WLDCORE_TEXT_ID_RUMOR_ABOUT = 0xb809,           /* "This rumor's about ..." */
    WLDCORE_TEXT_ID_NO_RUMORS = 0xb80a,             /* "Haven't heard any rumors yet." */
    WLDCORE_TEXT_ID_PROPOSITIONS_INTRO = 0xb80b,    /* "I know a great way to make a buck." */
    WLDCORE_TEXT_ID_PROPOSITION_ABOUT = 0xb80c,     /* "This's what I know about ..." */
    WLDCORE_TEXT_ID_NO_PROPOSITIONS = 0xb80d,       /* "Haven't heard any offers lately." */
    WLDCORE_TEXT_ID_NO_MORE_JOBS = 0xb80e,          /* "I can't give anymore jobs." */
    WLDCORE_TEXT_ID_ACCEPT_JOB_PROMPT = 0xb80f,     /* "You want this job?" */
    WLDCORE_TEXT_ID_JOB_FEE_PROMPT = 0xb810,        /* "You need ... gil for info etc. Still wanna do it?" */
    WLDCORE_TEXT_ID_NOT_ENOUGH_GIL = 0xb811,        /* "You don't got enough gil." */
    WLDCORE_TEXT_ID_CHOOSE_UNITS = 0xb812,          /* "So, who you wanna send?" */
    WLDCORE_TEXT_ID_CHOOSE_DAYS = 0xb813,           /* "This job's from ... to .... How many days you want?" */
    WLDCORE_TEXT_ID_CONFIRM_CONDITIONS = 0xb814,    /* "These are the conditions, OK?" */
    WLDCORE_TEXT_ID_JOBS_RETURNED = 0xb819,         /* "Good job. You came back from these jobs." */
    WLDCORE_TEXT_ID_HEAR_ABOUT_JOB = 0xb81a,        /* "Wanna hear about ...?" */
    WLDCORE_TEXT_ID_JOBS_IN_PROGRESS = 0xb81b,      /* "These jobs're in progress." */
    WLDCORE_TEXT_ID_RETURN_TOMORROW = 0xb81c,       /* "I'll have them return tomorrow." */
    WLDCORE_TEXT_ID_CALL_BACK_PROMPT = 0xb81d,      /* "What a waste if they quit now. Still wanna call them back?" */
    WLDCORE_TEXT_ID_CALL_BACK_ACCEPTED = 0xb81e,    /* "Ok, I'll have them return by tomorrow." */
    WLDCORE_TEXT_ID_JOB_CONDITION_LABELS = 0xb821,  /* "Days", "Place", "Unit" with the first two filled in */
    WLDCORE_TEXT_ID_CHOOSE_MORE_UNITS = 0xb822,     /* "Who else are you sending?" */
    WLDCORE_TEXT_ID_DAYS_LABEL = 0xb823,            /* "Days:" */
    WLDCORE_TEXT_ID_PARTY_NUMBER_LABEL = 0xb826,    /* "Party number:" */
    WLDCORE_TEXT_ID_JOB_RESULT_LABELS = 0xb830,     /* "Bonus", "Total", "Job Point" */
    WLDCORE_TEXT_ID_FOUR_DIGIT_VALUE = 0xb831,      /* a value, four digits wide */
    WLDCORE_TEXT_ID_QUIT_CONTINUE = 0xb83a,         /* "Quit / Continue" */
    WLDCORE_TEXT_ID_BRAVE_STORY_ROW_FIRST = 0xb83f, /* plus wldcore_brave_story_row_e: "Fact" to "Return" */
    WLDCORE_TEXT_ID_INJURED_CASUALTIES = 0xb846,    /* "Injured", "Casualties" and their counts */
    WLDCORE_TEXT_ID_BRAVE_STORY_INTRO = 0xb848, /* Alazlam: "The secrets of the Lion War are uncovered in this book." */
    WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO = 0xb849, /* "This is the information on ..." */
    WLDCORE_TEXT_ID_BRAVE_STORY_GOODBYE = 0xb84a,    /* "You must find truth on your own. ..." */
    WLDCORE_TEXT_ID_SOUND_NOVEL_RESUME_CHOICES
    = 0xb84b,                                    /* "This book was canceled earlier. Restart / Read from start" */
    WLDCORE_TEXT_ID_SUBSTITUTED_TEXT = 0xb84c,   /* the text of substitution value 0 alone */
    WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL = 0xb84e, /* "Job contents:" */
    WLDCORE_TEXT_ID_MONTH_NAME_BASE = 0xb84f,    /* plus the month, 1 to 12: "Jan" to "Dec" */
    WLDCORE_TEXT_ID_TOLD_BEFORE = 0xb85c,        /* "Think I told you before but..." */
    WLDCORE_TEXT_ID_BAR = 0xb85d,                /* the four entries of a town's location menu */
    WLDCORE_TEXT_ID_SHOP = 0xb85e,
    WLDCORE_TEXT_ID_SOLDIER_OFFICE = 0xb85f,
    WLDCORE_TEXT_ID_FUR_SHOP = 0xb860,
    WLDCORE_TEXT_ID_PROPOSITION_CATEGORY_ROW_FIRST
    = 0xb864,                                 /* plus wldcore_proposition_category_row_e: "Advancing", "Work history" */
    WLDCORE_TEXT_ID_VALUE = 0xb866,           /* a value */
    WLDCORE_TEXT_ID_TWO_DIGIT_VALUE = 0xb867, /* a value, two digits wide */
    WLDCORE_TEXT_ID_RAMZA_INFO = 0xb868,      /* BRAVE_STORY_ENTRY_INFO for person 0: a title and Ramza's name */
    WLDCORE_TEXT_ID_RAMZA_TITLE_FIRST = 0xb869,      /* plus his profile: sixteen titles, "Knight" each */
    WLDCORE_TEXT_ID_PROPOSITION_HINT_FIRST = 0xb879, /* plus the proposition: the bar owner's advice on it */
    WLDCORE_TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST
    = 0xb8d9,                                   /* plus wldcore_tutorial_category_row_e: "Organization" to "Return" */
    WLDCORE_TEXT_ID_TUTORIAL_WELCOME = 0xb8dd,  /* Daravon: "If you know, you can foresee. ..." */
    WLDCORE_TEXT_ID_TUTORIAL_PROMPT = 0xb8de,   /* "Know the path to win the battle. Solve your questions here." */
    WLDCORE_TEXT_ID_CHOOSE_HUMAN_UNIT = 0xb8e0, /* "Choose a good human to send, or one with no strings!" */
    WLDCORE_TEXT_ID_DEEP_DUNGEON_FLOOR_FIRST = 0xb8ed, /* plus the floor: "Nogias" to "End" */
    WLDCORE_TEXT_ID_TREASURE_FOUND_FIRST
    = 0xb8f9, /* the eleven messages for a treasure found, to "Treasure Hunter Master" */
    WLDCORE_TEXT_ID_LAND_FOUND_FIRST = 0xb904, /* the same for an unexplored land, to "Master Adventurer" */
} wldcore_text_id_e;

/* wldcore_menu_sound_novel_history_level_t.phase. */
typedef enum wldcore_sound_novel_history_phase {
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE = 0,
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM = 1,     /* five steps: the picture darkens under the fill box */
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE = 2, /* the reverse ramp, then the level pops */
} wldcore_sound_novel_history_phase_e;

/* wldcore_menu_sound_novel_level_t.phase. */
typedef enum wldcore_sound_novel_phase {
    WLDCORE_SOUND_NOVEL_PHASE_RUN = 0,          /* the script runs */
    WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY = 2, /* square: counts down, then pushes the history level */
    WLDCORE_SOUND_NOVEL_PHASE_END = 3,          /* the script ended: waits for thread 14, then leaves */
} wldcore_sound_novel_phase_e;

/* opcode */
extern wldcore_opcode_state_t g_wldcore_opcode_state;
extern void (*g_wldcore_opcode_handlers[])(void);
void wldcore_opcode_apply_pending_delta_and_clamp_value(void);
void wldcore_opcode_map_request_deep_dungeon_entry(void);
void wldcore_opcode_map_request_draw_path(void);
void wldcore_opcode_map_request_erase_path(void);
void wldcore_opcode_map_request_load_event(void);
void wldcore_opcode_map_request_set_var_2(void);
void wldcore_opcode_map_request_text(void);
void wldcore_opcode_start_pending_value_transition(void);
void wldcore_opcode_load_picture(void);
void wldcore_opcode_load_background_set(void);

/* sound */
/* Rows of four sound numbers in the WLDCORE sound bank (id 0xE1), indexed by
 * a sound id from 1. Row 0 is not a sound row: 0x8009ec18 holds the eighth
 * 32-bit value of g_wldcore_tutorial_event_ids (0x1a8). */
extern u8 g_wldcore_sound_quad_command_bytes[][4];
extern s32 g_wldcore_sound_novel_script_sector_offsets[];
extern s32 g_wldcore_sound_novel_text_sector_offsets[];
extern struct suzuki_sound_resource* g_wldcore_sound_novel_sound_resource;
extern s32 g_wldcore_sound_novel_countdown_frames;
extern s32 g_wldcore_sound_novel_countdown_section;
extern s32 g_wldcore_sound_novel_countdown_elapsed;
extern u32 g_wldcore_sound_novel_local_flags[];
extern s32 g_wldcore_sound_novel_wait_counter;
/* Unsigned views of g_wldcore_active_saved_record.text_history_0..3 for the
 * two history shifters, which load them with lhu; the record keeps s16 for
 * the -1 stores of wldcore_reset_saved_record_fields. */
extern s16 g_wldcore_sound_novel_text_history_0;
extern u16 g_wldcore_sound_novel_text_history_1;
extern u16 g_wldcore_sound_novel_text_history_2;
extern u16 g_wldcore_sound_novel_text_history_3;
extern s32 g_wldcore_sound_novel_wait_frames;
extern u8 g_wldcore_sound_novel_quit_help_text_ids[];
void wldcore_sound_process_audio_queue(void);
void wldcore_sound_enqueue_audio_command(wldcore_audio_command_e command, s32 value);
void wldcore_sound_enqueue_music_start(s32 value);
void wldcore_sound_play_effect(s32 sound_id);
void wldcore_sound_wait_for_queue_drain(void);
void wldcore_sound_novel_restore_saved_state(s32 render_index);

/* gfx */
extern s32 g_wldcore_loaded_background_set;
/* Currently resident picture set, reset to -1 by wldcore_init_world_map_state. */
extern s32 g_wldcore_loaded_picture_set;

/* save */
extern s16 g_wldcore_saved_record_index;
/* Saved copy of g_wldcore_context_value_display_position. */
extern point32_t g_wldcore_saved_context_value_display_position;
/* g_main_system_flags as saved by wldcore_gfx_toggle_captured_world_frame (main_system_flags_e). */
extern u32 g_wldcore_saved_system_flags;

/* script */
extern u16* g_wldcore_script_base;
extern void (*g_wldcore_script_opcode_table[])(void);
extern wldcore_script_state_t g_wldcore_script_state;
/* Scalar view of g_wldcore_script_state.flags for the opcode handlers that
 * read-modify-write it: the member spelling there shares one base register
 * with the args/data accesses instead of the target's absolute addresses.
 * The load_* opcode handlers declare volatile views of flags/data/ip locally. */
extern s32 g_wldcore_script_flags;
s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask);
s32 wldcore_script_run_until_yield(void);
u32 wldcore_sound_novel_find_section_instruction(s32 section, s32 opcode);

/* text */
extern s32 g_wldcore_loaded_message_block;
void wldcore_text_get_padded_dimensions(s32 text_id, size32_t* output);
void wldcore_text_init_scrollable_window(wldcore_text_scrollable_window_t* state);
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* window);

#endif
