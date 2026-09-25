#ifndef FFT_WLDCORE_H
#define FFT_WLDCORE_H

#include "fft/data.h"
#include "fft/main_file.h"
#include "fft/menu_types.h"
#include "psx/gs.h"
#include "psx/gte.h"
#include "psx/types.h"

/* The world-conditional interpreter clears this result word before each
 * block. Comparisons set CONDITION_FAILED when their predicate rejects the
 * block. Action opcodes set ACTION_REACHED to end the block and set an action
 * bit when a request is staged. */
typedef enum wldcore_script_result_flags {
    WLDCORE_SCRIPT_RESULT_ACTION_REACHED = 0x001,
    WLDCORE_SCRIPT_RESULT_CONDITION_FAILED = 0x002,
} wldcore_script_result_flags_e;

typedef enum wldcore_script_action_flags {
    WLDCORE_SCRIPT_ACTION_CHOICE = 0x004,
    WLDCORE_SCRIPT_ACTION_LOAD_EVENT = 0x008,
    WLDCORE_SCRIPT_ACTION_TEXT = 0x010,
    WLDCORE_SCRIPT_ACTION_DEEP_DUNGEON_ENTRY = 0x020,
    WLDCORE_SCRIPT_ACTION_SET_VAR_2 = 0x040,
    WLDCORE_SCRIPT_ACTION_DRAW_PATH = 0x080,
    WLDCORE_SCRIPT_ACTION_ERASE_PATH = 0x100,
    WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION = 0x200,
    WLDCORE_SCRIPT_ACTION_DRAW_LOCATION = 0x400,
    WLDCORE_SCRIPT_ACTION_ERASE_LOCATION = 0x800,
    WLDCORE_SCRIPT_ACTION_PATH_MASK = WLDCORE_SCRIPT_ACTION_DRAW_PATH | WLDCORE_SCRIPT_ACTION_ERASE_PATH,
    WLDCORE_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK
    = WLDCORE_SCRIPT_ACTION_DRAW_LOCATION | WLDCORE_SCRIPT_ACTION_ERASE_LOCATION,
    WLDCORE_SCRIPT_ACTION_MAP_MENU_MASK = WLDCORE_SCRIPT_ACTION_PATH_MASK | WLDCORE_SCRIPT_ACTION_FOCUS_LOCATION
        | WLDCORE_SCRIPT_ACTION_LOCATION_VISIBILITY_MASK,
} wldcore_script_action_flags_e;

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

typedef enum wldcore_scroll_limits {
    WLDCORE_SCROLL_MIN_X = -116,
    WLDCORE_SCROLL_MAX_X = 128,
    WLDCORE_SCROLL_MIN_Y = -64,
    WLDCORE_SCROLL_MAX_Y = 80
} wldcore_map_scroll_limits_e;

/* Byte view of the four-byte world-script instruction word at 0x800d4848:
 * opcode byte followed by up to three operand bytes. */
typedef struct wldcore_opcode_instruction_bytes {
    u8 opcode;
    u8 operand_0;
    u8 operand_1;
    u8 operand_2;
} wldcore_opcode_instruction_bytes_t;

typedef char wldcore_instruction_bytes_size_must_be_4[sizeof(wldcore_opcode_instruction_bytes_t) == 4 ? 1 : -1];

/* The world-script state from the instruction word at 0x800d4848 onwards
 * (the tail of wldcore_world_script_record_t). Handlers must reach it as
 * in-struct references: the target keeps each state_flags store ahead of the
 * window/render-record updates and reloads render_index after them, which GCC
 * only does for in-struct accesses. g_wldcore_opcode_instruction and
 * g_wldcore_state_flags stay scalar views of the first two words for the
 * handlers whose accesses must be scalar. */
typedef struct wldcore_opcode_state {
    union {
        u32 word;
        wldcore_opcode_instruction_bytes_t bytes;
    } instruction;   /* 0x00 */
    u16 state_flags; /* 0x04; g_wldcore_state_flags */
    u8 unknown_06[0x0e];
    s16 render_index; /* 0x14; g_wldcore_sound_novel_picture_render_index */
    s16 picture;      /* 0x16; g_wldcore_sound_novel_picture_id */
    u8 unknown_18[0x0e];
    s16 x; /* 0x26 */
    s16 y; /* 0x28 */
} wldcore_opcode_state_t;

/* Nine-byte active-proposition records at 0x80057eec. Daily update 0x80074b30
 * advances elapsed_days and sets completion bit 0x04. The assignment at
 * 0x80074a34 stores the current world-map location at +0x04; 0x80074c40
 * filters completed records by that byte. */
typedef struct wldcore_proposition_progress {
    u8 flags;
    u8 proposition_id; /* 0x01; text 0xd000 | id names it (scus_94221_datatables.txt 80057eec "Prop ID") */
    u8 elapsed_days;
    u8 assigned_days;
    u8 location;
    u8 participant_count;      /* 0x05; speaker selection at 0x800795c4 trusts this count */
    u8 participant_indices[3]; /* 0x06; 0x80074a8c passes each index to party lookup before 0x80074ab4 stores it */
} wldcore_proposition_progress_t;

typedef char wldcore_proposition_progress_size_must_be_9[sizeof(wldcore_proposition_progress_t) == 9 ? 1 : -1];

/* Descriptor prefix read by 0x8007df84. The word-indexed text table is at
 * a record-relative offset; no fixed whole-record size is established. */
typedef struct wldcore_window_text_table {
    u8 unknown_00[8];
    s32 window_index; /* 0x08; renderer 0x8008aea0 indexes 52-byte windows */
    u8 unknown_0c[16];
    s32 table_word_offset; /* 0x1c */
} wldcore_window_text_table_t;

typedef struct wldcore_text_dimensions {
    s32 width;
    s32 height;
} wldcore_text_dimensions_t;

/* Caller-owned message state prefix at 0x800794d0. Only the phase word at
 * +0x04 is established; this is not the full allocation's size. */
typedef struct wldcore_proposition_message_state {
    u8 unknown_00[4];
    s32 phase;
} wldcore_proposition_message_state_t;

/* Only the nine-byte message-adjustment prefix used from proposition record 2
 * is established. Other proposition records may extend this layout. */
typedef struct wldcore_proposition_data {
    u8 message_adjustments[9];
} wldcore_proposition_data_t;

typedef char wldcore_text_table_offset_must_be_0x1c
    [((unsigned long)&((wldcore_window_text_table_t*)0)->table_word_offset == 0x1c) ? 1 : -1];

typedef char wldcore_text_dimensions_size_must_be_8[(sizeof(wldcore_text_dimensions_t) == 8) ? 1 : -1];

typedef struct wldcore_menu_cursor_state {
    s32 selected_index;
    s32 scroll_offset;
} wldcore_menu_cursor_state_t;

typedef char wldcore_menu_cursor_state_size_must_be_8[sizeof(wldcore_menu_cursor_state_t) == 8 ? 1 : -1];

/* Saved-data records at 0x80059594. The exact caller at 0x8008aba8 establishes
 * the 0x38 stride; 0x8008abe4 selectively resets these fields. Field meanings
 * beyond those stores remain provisional. */
typedef struct wldcore_saved_record {
    /* 0x00: the slot's save-block buffer; MAIN's initializer at 0x80059594
     * stores g_main_save_slot_buffer (0x800d0bbc) here for all five slots. */
    u8* buffer;
    u32* script_words; /* 0x04 */
    union {
        u32 word;
        wldcore_opcode_instruction_bytes_t bytes;
    } instruction; /* 0x08 */
    /* 0x0c-0x37 mirror the live sound-novel state at 0x800d484c. */
    u16 state_flags; /* 0x0c */
    s16 section;     /* 0x0e */
    s16 word_index;  /* 0x10; instruction word index in the section; cleared by wldcore_sound_novel_restore_saved_state
                      */
    s16 counter_delta;        /* 0x12 */
    s16 text_id;              /* 0x14 */
    s16 brightness;           /* 0x16 */
    s16 field_18;             /* 0x18 */
    s16 background_set;       /* 0x1a */
    s16 picture_render_index; /* 0x1c */
    s16 picture_id;           /* 0x1e */
    s16 text_history_0;       /* 0x20; four-entry text history, newest last */
    s16 text_history_1;       /* 0x22 */
    s16 text_history_2;       /* 0x24 */
    s16 text_history_3;       /* 0x26 */
    u16 value_display_mode;   /* 0x28 */
    u16 music_selection;      /* 0x2a */
    u16 sound_mode;           /* 0x2c */
    s16 picture_x;            /* 0x2e */
    s16 picture_y;            /* 0x30 */
    s16 sound_id;             /* 0x32 */
    s32 counter;              /* 0x34 */
} wldcore_saved_record_t;

typedef char wldcore_saved_record_size_must_be_0x38[sizeof(wldcore_saved_record_t) == 0x38 ? 1 : -1];

/* 36-byte window records at 0x800bb504: animated display objects. The pool
 * init 0x8006ad28 sets flags = 1, so the display-object dispatcher 0x8006ae20
 * hands them to wldcore_gfx_draw_animated_display_object (0x8006aed0), which
 * reads them as wldcore_anim_object_t. 0x8009036c initialises a record
 * (flags |= 0x810, sequence/priority from arguments, frame_index and
 * anim_counter cleared); 0x80090a28 toggles flag 0x10 by record index. */
typedef struct wldcore_window_record {
    u32 flags;        /* 0x00 */
    s32 sequence;     /* 0x04; row of g_wldcore_anim_sequence_table; second argument of 0x8009036c */
    s32 priority;     /* 0x08; OT priority passed to the sprite sort; first argument of 0x8009036c */
    s32 frame_index;  /* 0x0c; frame within the sequence, cleared with anim_counter on every sequence change */
    s32 anim_counter; /* 0x10; frames spent on the current frame */
    /* 0x14; CLUT row offset: 0 keeps the part's CLUT, otherwise cx = cx_base + (palette - 1) << 4. The
     * window-state routines at 0x80076744/0x8007688c store 0, 6 or 10. */
    s32 palette;
    s32 x;
    s32 y;
    s32 color; /* 0x20; rgb tint word, 0x80 grey from the pool init and the append routines */
} wldcore_window_record_t;

typedef char wldcore_window_record_size_must_be_0x24[sizeof(wldcore_window_record_t) == 0x24 ? 1 : -1];

/* Position view starting at a window record's x field with its 0x24-byte stride. */
typedef struct wldcore_window_record_position_view {
    s32 x;
    s32 y;
    u8 stride_padding[0x1c];
} wldcore_window_record_position_view_t;

/* Window-layout request at the start of a layout-window menu level. The
 * first three words are what wldcore_window_apply_layout (0x8006f67c) reads. */
typedef struct wldcore_window_layout_request {
    s32 window_index;
    s32 layout; /* 0x04; row of g_wldcore_window_layout_rows */
    s32 cached_layout_kind;
    s32 saved_state; /* 0x0c; previous g_wldcore_menu_ordering_table_offset, saved by 0x8006f528 */
} wldcore_window_layout_request_t;

/* Provisional: 16-byte rows of the window-layout table at 0x8009e93c.
 * wldcore_menu_handle_layout_window_input reads the first ten bytes: a pointer
 * to the level's live counter, the base message id, and the four neighbouring
 * layout ids reached by the directional pad. wldcore_window_apply_layout reads
 * the trailing halfwords: the record's x and y extents (plus 14 and 12) and the
 * kind that selects the redraw delay. */
typedef struct wldcore_window_layout_row {
    u16* counter;     /* 0x00 */
    u16 message_base; /* 0x04 */
    u8 up;            /* 0x06 */
    u8 down;          /* 0x07 */
    u8 left;          /* 0x08 */
    u8 right;         /* 0x09 */
    s16 x;            /* 0x0a */
    s16 y;            /* 0x0c */
    s16 kind;         /* 0x0e */
} wldcore_window_layout_row_t;

typedef char wldcore_window_layout_row_size_must_be_0x10[sizeof(wldcore_window_layout_row_t) == 0x10 ? 1 : -1];

/* Message menu level pushed by 0x8006e860. */
typedef struct wldcore_menu_message_level {
    s32 reload_text;
    s32 saved_menu_result; /* 0x04; g_wldcore_menu_result at push time */
    s32 window_index;      /* 0x08; window record appended by the type-2 push; cleared by the type-3 push */
    s32 field_0c;          /* 0x0c; set to 8 at push */
} wldcore_menu_message_level_t;

/* Panel menu levels: number panels (0x8006f294, 0x80073ef8) and list panels
 * (0x8007dfcc, 0x80084230, 0x80071aa0). */
typedef struct wldcore_menu_panel_level {
    /* 0x00; 1 confirm, 0 or -1 cancel, set by the list and number-panel steps and branched on once
     * WORLD thread 12 ends; cleared by the number-panel pushes */
    s32 result;
    s32 variable_id; /* 0x04; script variable edited by number panels */
    u8 unknown_08[0x28];
    s32 entry_count; /* 0x30; rows in the panel list */
    /* 0x34; entry text ids, filled by wldcore_map_build_location_menu_entries (0x8008d2c8), whose result is
     * the entry count; the same offset as the list_window and proposition_list entries */
    s32 entries[10];
} wldcore_menu_panel_level_t;

/* Provisional: the type-1 world-map close/return level pushed by
 * wldcore_menu_push_entry_window_level. wldcore_menu_step_return_level
 * (0x8006e208) is the only reader: delay counts down the frames the fade
 * needs and pending latches while system flag 0x04000000 is up. */
typedef struct wldcore_menu_return_level {
    s32 delay;
    s32 pending; /* 0x04 */
} wldcore_menu_return_level_t;

/* Menu level owning a window record and a render record; 0x80088180 clears
 * both records' flag 0x10 and sets render flag 0x100 when it pops back to it. */
typedef struct wldcore_menu_window_render_level {
    s32 window_index;
    s32 render_index; /* 0x04 */
} wldcore_menu_window_render_level_t;

/* Menu level owning two window records; 0x8008b8dc toggles both records' flag 0x10. */
typedef struct wldcore_menu_window_pair_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
} wldcore_menu_window_pair_level_t;

/* Menu level owning two window records and a render record: 0x800712b0
 * pushes it (type 5); the tutorial text window 0x80084578 hides all three
 * with flag 0x10 and 0x80084748 restores them. */
typedef struct wldcore_menu_window_pair_render_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 field_0c;      /* 0x0c; cleared by 0x800712b0 */
} wldcore_menu_window_pair_render_level_t;

/* Provisional: countdown menu object stepped by wldcore_menu_show_next_sequence_message
 * (0x8007a6b8) and driven by 0x8007a86c. The stepper sets phase 1 and clears
 * the timer when the count expires; 0x8007a86c then advances phases 1-3. */
typedef struct wldcore_menu_countdown {
    s32 current;
    s32 remaining;
    s32 phase;
    s32 timer; /* 0x0c; frames spent in phase 2 */
} wldcore_menu_countdown_t;

/* Provisional: list-window level beneath the flag-list panels; 0x80087b94
 * and 0x8008343c hide its content render record (flag 0x10) when they push.
 * 0x800928ac clears flag 0x10 on the upper and lower windows when it pops
 * back to it. */
typedef struct wldcore_menu_list_window_level {
    s32 main_window;
    s32 side_window;    /* 0x04 */
    s32 frame_render;   /* 0x08 */
    s32 content_render; /* 0x0c */
    u8 unknown_10[8];
    s32 mode;           /* 0x18; cleared by the 0x80080e54 push */
    s32 selected_entry; /* 0x1c; initial row from g_wldcore_brave_story_saved_cursor */
    s32 upper_window;   /* 0x20 */
    s32 lower_window;   /* 0x24 */
    u8 unknown_28[8];
    s32 entry_count; /* 0x30; rows drawn by 0x80080e54 */
    s32 entries[10]; /* 0x34; entry kinds, text id 0xb83f + kind */
} wldcore_menu_list_window_level_t;

/* Rumor detail level pushed at 0x80072618. The close handler at 0x80072888
 * reads the saved parent window/render indices at +0x08/+0x0c/+0x10. */
typedef struct wldcore_menu_rumor_detail_level {
    s32 rumor_index;
    s32 field_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} wldcore_menu_rumor_detail_level_t;

/* Provisional: proposition list level (type 0x26) pushed by 0x80085760: a
 * cursor window and three column-header windows over one render record
 * listing each active proposition's name, participant count and remaining
 * days. */
typedef struct wldcore_menu_proposition_list_level {
    s32 cursor_window; /* 0x00; window sequence 2, moved to the cursor row */
    s32 title_window;  /* 0x04; window sequence 0x63 */
    s32 count_window;  /* 0x08; window sequence 0x64 */
    s32 days_window;   /* 0x0c; window sequence 0x65 */
    s32 render_index;  /* 0x10 */
    u8 unknown_14[8];
    s32 cursor; /* 0x1c; initial row from g_wldcore_active_propositions_saved_cursor */
    u8 unknown_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34; indices into g_main_active_propositions */
} wldcore_menu_proposition_list_level_t;

/* Provisional: debug script-variable list level (type 0xb) pushed by
 * 0x8006ea90; 0x8006ed30 draws row_count variables from first_variable
 * into the render record. */
typedef struct wldcore_menu_variable_list_level {
    s32 window_index;   /* 0x00; cursor window */
    s32 render_index;   /* 0x04 */
    s32 first_variable; /* 0x08 */
    s32 cursor;         /* 0x0c; cursor row */
    u8 unknown_10[0x20];
    s32 row_count; /* 0x30 */
} wldcore_menu_variable_list_level_t;

/* Provisional: proposition participant detail level (type 0x28) pushed by
 * 0x80086f1c with two frame windows; wldcore_unit_build_status_panel_data draws the participant
 * g_main_active_propositions[proposition].participant_indices[participant]. */
typedef struct wldcore_menu_participant_level {
    s32 left_window;  /* 0x00 */
    s32 right_window; /* 0x04 */
    s32 field_08;     /* 0x08; cleared at push */
    s32 field_0c;     /* 0x0c; cleared at push */
    s32 proposition;  /* 0x10; index into g_main_active_propositions */
    s32 participant;  /* 0x14; slot in participant_indices */
} wldcore_menu_participant_level_t;

/* Provisional: script-variable detail level (type 0x20) pushed by
 * 0x8008389c over the list-window level; it keeps the selected entry value
 * (variable index * 16 + value) and the render record it appends. */
typedef struct wldcore_menu_variable_detail_level {
    s32 value;        /* 0x00 */
    s32 render_index; /* 0x04 */
    s32 field_08;
    s32 phase; /* 0x0c; set to 1 at push; the detail steps switch on it */
    s32 timer; /* 0x10; cleared at push; text delay or dissolve step, counted per frame */
} wldcore_menu_variable_detail_level_t;

/* Provisional: the type-0x23 event-transition level pushed by
 * wldcore_menu_push_event_transition_level (0x8008047c), which stores its
 * argument at +0x20, and stepped by wldcore_menu_step_event_transition_level;
 * wldcore_draw_close_indicator_if_pending draws the indicator sprite at
 * 0x8009f288 while the parent level's mode is not 2. */
typedef struct wldcore_menu_event_transition_level {
    s32 frame;           /* 0x00; counts step frames; the sprite updates every third, 1 restores the captured screen */
    s32 drift_direction; /* 0x04; rand() * 4 >> 15 at push; bits 0/1 pick the sprite's x/y drift sign */
    s32 spin_direction;  /* 0x08; rand() * 2 >> 15 at push; selects +/-0x2000 rotation per update */
    s32 phase;           /* 0x0c; 0 spin/scale in, 1 spin out, 2 done */
    s32 fade_phase;      /* 0x10; 0 none, 1 fade overlay ramps by 0x10, 2 ramps by 8 and restores the screen */
    s32 anim_step;       /* 0x14; +2/+4 per update; drives scale, shade, sounds 0x6c/0x6d and phase changes */
    u8 unknown_18[8];
    s32 mode; /* 0x20; argument of 0x8008047c; 2 fades the screen instead */
} wldcore_menu_event_transition_level_t;

/* Type-0x14 sound-novel resume level pushed by
 * wldcore_menu_push_sound_novel_resume_level and stepped by
 * wldcore_menu_step_sound_novel_resume_level: a cursor window over the
 * saved-picture render record and two text render records. Only mode shares
 * its meaning with the type-0x23 level at the same offset. */
typedef struct wldcore_menu_sound_novel_resume_level {
    s32 cursor_window;  /* 0x00; moved between the two option rows */
    s32 picture_render; /* 0x04; VRAM render record of the TIM at g_wldcore_picture_buffer */
    s32 message_render; /* 0x08; text 0xb84c */
    s32 option_render;  /* 0x0c; text 0xb84b; the cursor rows are relative to its base */
    s32 dissolve_step;  /* 0x10; step of the progressive picture upload, 0 once it finishes */
    s32 cursor_row;     /* 0x14; 0 or 1, toggled by up/down; 1 resets the selected saved record */
    u8 unknown_18[8];
    s32 mode; /* 0x20; argument of the push */
} wldcore_menu_sound_novel_resume_level_t;

/* Provisional: type-0x31 screen-transition level pushed by
 * wldcore_menu_push_screen_transition_level (0x80088308) and stepped by
 * wldcore_menu_step_screen_transition_level. */
typedef struct wldcore_menu_screen_transition_level {
    s32 screen;   /* 0x00; screen type 0x1b..0x2d to reopen */
    s32 param_a;  /* 0x04 */
    s32 param_b;  /* 0x08 */
    s32 field_0c; /* 0x0c */
    s32 delay;    /* 0x10; 0x14 at push */
    s32 phase;    /* 0x14; cleared at push */
} wldcore_menu_screen_transition_level_t;

/* Provisional: the type-0x16 sound-novel save-confirmation level pushed by
 * wldcore_menu_push_sound_novel_quit_level and stepped by
 * wldcore_menu_step_sound_novel_quit_level. */
typedef struct wldcore_menu_save_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 field_0c;
    s32 fade_out; /* 0x10 */
    s32 fade_in;  /* 0x14 */
    s32 field_18; /* 0x18; cleared at push */
    s32 choice;   /* 0x1c */
} wldcore_menu_save_confirm_level_t;

/* Provisional: the type-0x17 history-text level pushed by
 * wldcore_menu_push_sound_novel_history_level, which stores the newest
 * history text at +0x0c, the three older ones at +0x10..+0x18, phase 1 and
 * step 0. wldcore_window_draw_indexed_text reads the same record through
 * wldcore_window_text_table_t (scroll at +0x1c). */
typedef struct wldcore_menu_sound_novel_history_level {
    s32 upper_window;   /* 0x00 */
    s32 lower_window;   /* 0x04 */
    s32 render_index;   /* 0x08 */
    s32 newest_text_id; /* 0x0c; newest history text */
    s32 text_ids[3];    /* 0x10 */
    s32 scroll;         /* 0x1c */
    s32 phase;          /* 0x20; 0 input, 1 fading in, 2 fading out */
    s32 step;           /* 0x24; 0..4 fade step */
} wldcore_menu_sound_novel_history_level_t;

/* Provisional: location-transition menu level. The handler at 0x8008e540
 * uses the leading window index, location id, pending result, and the
 * transition-data address. */
typedef struct wldcore_menu_location_transition_level {
    s32 window_index; /* 0x00 */
    u8 unknown_04[8];
    s32 location_id; /* 0x0c */
    u8 unknown_10[0x20];
    s32 pending_result;       /* 0x30 */
    u8 transition_data[0x28]; /* 0x34 */
} wldcore_menu_location_transition_level_t;

typedef char wldcore_menu_location_transition_level_size_must_be_0x5c
    [sizeof(wldcore_menu_location_transition_level_t) == 0x5c ? 1 : -1];

/* Provisional: proposition send-unit menu level. The handler at 0x80075bd8
 * selects one of three party slots and owns its portrait render records. */
typedef struct wldcore_menu_send_unit_level {
    s32 list_window;    /* 0x00 */
    s32 layout_render;  /* 0x04 */
    s32 frame_window_a; /* 0x08 */
    s32 frame_window_b; /* 0x0c */
    s32 preview_render; /* 0x10 */
    s32 list_render;    /* 0x14 */
    s32 cursor_window;  /* 0x18 */
    u8 unknown_1c[4];
    s32 mode;          /* 0x20 */
    s32 shown_preview; /* 0x24 */
    s32 slot;          /* 0x28 */
    u8 unknown_2c[4];
    s32 unit_count;      /* 0x30; wrap bound, written by wldcore_proposition_load_send_unit_candidates */
    s32 portrait_render; /* 0x34 */
    s32 portrait_frame;  /* 0x38 */
    s32 portrait_window; /* 0x3c */
    s32 left_arrow;      /* 0x40 */
    s32 right_arrow;     /* 0x44 */
    u8 unknown_48[0x10];
    s32 shown_unit; /* 0x58 */
} wldcore_menu_send_unit_level_t;

typedef char wldcore_menu_send_unit_level_size_must_be_0x5c[sizeof(wldcore_menu_send_unit_level_t) == 0x5c ? 1 : -1];

/* The two-choice confirmation panel built by wldcore_window_build_yes_no_panel and stepped by
 * wldcore_proposition_handle_accept_confirm_input and wldcore_proposition_handle_dispatch_confirm_input. */
typedef struct wldcore_menu_choice_panel_level {
    s32 cursor_window; /* 0x00 */
    s32 render_index;  /* 0x04 */
    s32 row;           /* 0x08; 0 = yes, 1 = no */
    s32 mode;          /* 0x0c; 0 choosing, 1 and 3 waiting on a message */
    s32 header_window; /* 0x10 */
} wldcore_menu_choice_panel_level_t;

/* Provisional: the job-report confirmation level. The cursor window at +0x00
 * is moved between the two rows of the render record at +0x04, and +0x08
 * records which of the two rows is selected.
 * wldcore_list_handle_completed_propositions_input pushes this level (type
 * 0x0f) over the yes/no panel and stores the argument at +0x10. */
typedef struct wldcore_job_report_confirm_level {
    s32 cursor_window; /* 0x00; window record moved between the two rows */
    s32 render_index;  /* 0x04; render record holding the confirmation text */
    s32 selection;     /* 0x08; 0 = upper row, 1 = lower row */
    s32 field_0c;
    s32 argument; /* 0x10; argument of the level wldcore_menu_push_proposition_report_level pushes */
} wldcore_job_report_confirm_level_t;

/* Leading words of a menu level that owns up to two window/render record
 * pairs; wldcore_menu_close_level_and_release_records releases each index
 * that is not -1. */
typedef struct wldcore_menu_slot_set {
    s32 result;   /* 0x00; the location menu step sets 1 on confirm, 0 on cancel */
    s32 window_a; /* 0x04 */
    s32 render_a; /* 0x08 */
    s32 window_b; /* 0x0c */
    s32 render_b; /* 0x10 */
} wldcore_menu_slot_set_t;

/* Provisional: owner of a text-list render record, as
 * wldcore_list_build_tutorial_category_panel_image reads it; only the record
 * index, row count and text-id rows are established. */
typedef struct wldcore_text_list_panel {
    u8 unknown_00[8];
    s32 render_index; /* 0x08 */
    u8 unknown_0c[0x24];
    s32 row_count;   /* 0x30 */
    s32 text_ids[1]; /* 0x34; row_count entries, relative to text 0xb8d9 */
} wldcore_text_list_panel_t;

/* Provisional: the world-map cursor level stepped by
 * wldcore_menu_step_map_cursor_level. */
typedef struct wldcore_menu_map_cursor_level {
    s32 window_index;     /* 0x00; the map cursor window */
    s32 alt_window_index; /* 0x04; the wide-view window swapped in by 0x80 */
    s32 wide_view;        /* 0x08; non-zero while the wide view is shown */
    s32 snap_hit;         /* 0x0c; dot index returned by the snap probe */
    s32 pending_mode;     /* 0x10 */
} wldcore_menu_map_cursor_level_t;

/* The type-0x36 menu level pushed by wldcore_push_map_location_visibility_level
 * and stepped by wldcore_map_step_location_visibility_change; only the first
 * four words are known. */
typedef struct wldcore_map_visibility_level {
    s32 dot_index;     /* 0x00; map dot, and script variable 0x200 + dot_index */
    s32 was_set;       /* 0x04; whether the variable was already set */
    s32 delay;         /* 0x08; frames the step handler waits, 0x10 here */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 unknown_10[0x4c];
} wldcore_map_visibility_level_t;

typedef char wldcore_map_visibility_level_size_must_be_0x5c[sizeof(wldcore_map_visibility_level_t) == 0x5c ? 1 : -1];

/* Provisional: the location menu level as its step handler
 * wldcore_menu_step_location_menu_level sees it. */
typedef struct wldcore_menu_location_menu_level {
    wldcore_menu_slot_set_t slots; /* 0x00; slots.result is the confirmed choice */
    s32 delay;                     /* 0x14 */
    s32 fade;                      /* 0x18 */
    u8 unknown_1c[0x14];
    s32 entry_count; /* 0x30 */
} wldcore_menu_location_menu_level_t;

/* Provisional: the type-4 location list level as wldcore_menu_push_location_menu_level
 * fills it. */
typedef struct wldcore_location_list_level {
    s32 window_index;  /* 0x00 */
    s32 cursor_window; /* 0x04; -1 until the cursor record is appended */
    s32 frame_render;  /* 0x08; -1 when there is no picture */
    s32 label_window;  /* 0x0c; -1 when there is no picture */
    s32 extra_render;  /* 0x10; -1 at push; released on close when set (wldcore_menu_slot_set_t.render_b) */
    s32 delay; /* 0x14; 8 at push, counted down by the location step (wldcore_menu_location_menu_level_t.delay) */
    s32 mode;  /* 0x18; 0 then 8 */
    s32 selected_entry;
    u8 unknown_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34 */
} wldcore_location_list_level_t;

/* Provisional: the type-0x10 proposition report level pushed by
 * wldcore_menu_push_proposition_report_level (0x800779d0), which stores its argument at
 * +0x2c and starts WORLD thread 14, and stepped by
 * wldcore_proposition_step_report_level. */
typedef struct wldcore_proposition_report_level {
    s32 state; /* 0x00; step index, -1 .. 0x12; pushed as -1, or 0x63 when g_wldcore_job_selection.gate is set */
    s32 phase; /* 0x04; wldcore_proposition_step_message phase; cleared at push */
    u8 unknown_08[4];
    s32 load_phase;   /* 0x0c; 1 chapter data, 2 picture data; 1 at push */
    s32 render_index; /* 0x10; picture render record; -1 at push */
    u8 unknown_14[0x18];
    s32 proposition; /* 0x2c; argument of the push */
} wldcore_proposition_report_level_t;

/* Provisional: the type-0x15 sound-novel level pushed by
 * wldcore_menu_push_sound_novel_level and stepped by
 * wldcore_menu_step_sound_novel_level: title, header and footer windows
 * around a bar render record. */
typedef struct wldcore_menu_sound_novel_level {
    s32 left_window;    /* 0x00; title window */
    s32 right_window;   /* 0x04; header window */
    s32 render_index;   /* 0x08; bar render record */
    s32 overlay_window; /* 0x0c; footer window */
    u8 unknown_10[0xc];
    s32 message;    /* 0x1c; argument of the push */
    s32 unknown_20; /* 0x20; set to 1 at push */
    s32 countdown;  /* 0x24; 9999 at push */
    s32 phase;      /* 0x28; cleared at push */
    s32 fade_timer; /* 0x2c; cleared at push */
} wldcore_menu_sound_novel_level_t;

/* Provisional: proposition-result menu level (type 0x11) pushed by
 * wldcore_proposition_push_result_level and stepped by 0x80078bb8. */
typedef struct wldcore_proposition_result_level {
    s32 render_index; /* 0x00 */
    u8 unknown_04[0xc];
    s32 phase; /* 0x10; 0 waiting for input, 1 fading out, 2 finished */
    u8 unknown_14[0x14];
    s32 delay;       /* 0x28; frames before the success jingle restarts */
    s32 proposition; /* 0x2c; index into g_main_active_propositions */
    u8 unknown_30[0x2c];
} wldcore_proposition_result_level_t;

/* Provisional: the bar's active-proposition list level
 * (wldcore_bar_handle_active_propositions_input); the confirmed flag and the value
 * words indexed by the selected row are the only fields read. */
typedef struct wldcore_bar_active_propositions_level {
    s32 confirmed;
    u8 unknown_04[0x30];
    s32 values[1]; /* 0x34 */
} wldcore_bar_active_propositions_level_t;

/* Provisional: the type-0x35 menu level pushed by
 * wldcore_menu_push_map_path_level, decoded from the four words it writes and
 * corroborated by its step handler at
 * 0x8006dbb8, which reads only the last two: +0x08 selects the erase
 * direction and +0x0c is the one-shot sound flag. The two leading words are
 * the script's path arguments, which the caller
 * wldcore_menu_dispatch_pending_script_requests passes as g_wldcore_script_state.args[0]
 * and g_wldcore_script_state.args[1]. The record is one menu-stack slot, so the
 * view is 0x5c bytes wide and only the first four words are known. */
typedef struct wldcore_map_path_level {
    s32 path_id;       /* 0x00; g_wldcore_script_state.args[0] */
    s32 path_id_hi;    /* 0x04; g_wldcore_script_state.args[1] */
    s32 erase;         /* 0x08; step handler walks the path backwards when set */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 unknown_10[0x4c];
} wldcore_map_path_level_t;

typedef char wldcore_map_path_level_size_must_be_0x5c[sizeof(wldcore_map_path_level_t) == 0x5c ? 1 : -1];

/* Menu-stack level types: the g_wldcore_menu_stack_types value that selects a
 * level's idle, publish and resume handlers. Each value is stored by exactly
 * one push routine, which supplies its name; no routine pushes 0x01. */
typedef enum wldcore_menu_level_type {
    WLDCORE_MENU_LEVEL_ROOT = 0x00,
    WLDCORE_MENU_LEVEL_TEXT_WINDOW = 0x02,
    WLDCORE_MENU_LEVEL_MESSAGE = 0x03,
    WLDCORE_MENU_LEVEL_LOCATION_MENU = 0x04,
    WLDCORE_MENU_LEVEL_BAR_MENU = 0x05,
    WLDCORE_MENU_LEVEL_RUMORS = 0x06,
    WLDCORE_MENU_LEVEL_AVAILABLE_PROPOSITIONS = 0x07,
    WLDCORE_MENU_LEVEL_RUMOR_DETAIL = 0x08,
    WLDCORE_MENU_LEVEL_PROPOSITION_DETAIL = 0x09,
    WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT = 0x0a,
    WLDCORE_MENU_LEVEL_VARIABLE_LIST = 0x0b,
    WLDCORE_MENU_LEVEL_NUMBER_PANEL = 0x0c,
    WLDCORE_MENU_LEVEL_PROPOSITION_DISPATCH_CONFIRM = 0x0d,
    WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS = 0x0e,
    WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE = 0x0f,
    WLDCORE_MENU_LEVEL_PROPOSITION_REPORT = 0x10,
    WLDCORE_MENU_LEVEL_PROPOSITION_RESULT = 0x11,
    WLDCORE_MENU_LEVEL_PROPOSITIONS = 0x12,
    WLDCORE_MENU_LEVEL_PROPOSITION_CANCEL_CONFIRM = 0x13,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_RESUME = 0x14,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL = 0x15,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_QUIT = 0x16,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_HISTORY = 0x17,
    WLDCORE_MENU_LEVEL_UNEXPLORED_LANDS = 0x18,
    WLDCORE_MENU_LEVEL_UNEXPLORED_LAND_DETAIL = 0x19,
    WLDCORE_MENU_LEVEL_TREASURES = 0x1a,
    WLDCORE_MENU_LEVEL_TREASURE_DETAIL = 0x1b,
    WLDCORE_MENU_LEVEL_BRAVE_STORY = 0x1c,
    WLDCORE_MENU_LEVEL_STORY_EVENTS = 0x1d,
    WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT = 0x1e,
    WLDCORE_MENU_LEVEL_SET_SCRIPT_VARIABLES_03C0_03FF = 0x1f,
    WLDCORE_MENU_LEVEL_VARIABLE_DETAIL = 0x20,
    WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS = 0x21,
    WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT = 0x22,
    WLDCORE_MENU_LEVEL_EVENT_TRANSITION = 0x23,
    WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT = 0x24,
    WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY = 0x25,
    WLDCORE_MENU_LEVEL_ACTIVE_PROPOSITIONS = 0x26,
    WLDCORE_MENU_LEVEL_PARTICIPANT_LIST = 0x27,
    WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL = 0x28,
    WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS = 0x29,
    WLDCORE_MENU_LEVEL_PROPOSITION_ATTEMPT_DETAIL = 0x2a,
    WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES = 0x2b,
    WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS = 0x2c,
    WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES = 0x2d,
    WLDCORE_MENU_LEVEL_SCRIPT_FLAGS_01A4_01BB = 0x2e,
    WLDCORE_MENU_LEVEL_SCRIPT_FLAG_01A4_DETAIL = 0x2f,
    WLDCORE_MENU_LEVEL_NUMBER_PANEL_WITH_ARGUMENT = 0x30,
    WLDCORE_MENU_LEVEL_SCREEN_TRANSITION = 0x31,
    WLDCORE_MENU_LEVEL_FOCUS_LOCATION = 0x32,
    WLDCORE_MENU_LEVEL_SCROLLABLE_TEXT_WINDOW = 0x33,
    WLDCORE_MENU_LEVEL_FIXED_NUMBER_PANEL = 0x34,
    WLDCORE_MENU_LEVEL_MAP_PATH = 0x35,
    WLDCORE_MENU_LEVEL_MAP_LOCATION_VISIBILITY = 0x36,
    WLDCORE_MENU_LEVEL_COUNTDOWN = 0x37,
    WLDCORE_MENU_LEVEL_LAYOUT_WINDOW = 0x38,
} wldcore_menu_level_type_e;

/* Provisional: 0x5c-byte menu-stack records at 0x800bb930, one per menu
 * level; the level's handler type in g_wldcore_menu_stack_types selects the
 * view. The leading word is the level's window index wherever a generic
 * reader (0x8006c844, 0x8006d928, 0x8006c350) consumes it. g_wldcore_menu_stack_records_next is
 * the record array offset by one level, which the push routines index by the
 * current depth; spelling that as g_wldcore_menu_stack_records[depth + 1]
 * changes their address arithmetic. g_wldcore_menu_stack_types_next is the
 * same view of the type array, kept only where types[depth + 1] reschedules
 * the push routine (proposition detail, tutorial topic text, brave-story and
 * 0x03c0-0x03ff variable lists). */
typedef union wldcore_menu_stack_record {
    s32 window_index;
    wldcore_window_layout_request_t layout_window;
    wldcore_menu_message_level_t message;
    wldcore_menu_return_level_t return_level;
    wldcore_menu_panel_level_t panel;
    wldcore_menu_window_render_level_t window_render;
    wldcore_menu_window_pair_level_t window_pair;
    wldcore_menu_window_pair_render_level_t window_pair_render;
    wldcore_menu_countdown_t countdown; /* pushed by 0x8007a508 */
    wldcore_menu_rumor_detail_level_t rumor_detail;
    wldcore_menu_list_window_level_t list_window;
    wldcore_menu_proposition_list_level_t proposition_list;
    wldcore_menu_variable_list_level_t variable_list;
    wldcore_menu_participant_level_t participant;
    wldcore_menu_variable_detail_level_t variable_detail;
    wldcore_menu_event_transition_level_t event_transition;
    wldcore_menu_sound_novel_resume_level_t sound_novel_resume;
    wldcore_menu_screen_transition_level_t screen_transition;
    wldcore_menu_save_confirm_level_t save_confirm;
    wldcore_menu_sound_novel_history_level_t sound_novel_history;
    wldcore_menu_location_transition_level_t location_transition;
    wldcore_menu_send_unit_level_t send_unit;
    wldcore_menu_choice_panel_level_t choice_panel;
    wldcore_job_report_confirm_level_t job_report_confirm;
    wldcore_text_list_panel_t text_list;
    wldcore_menu_map_cursor_level_t map_cursor;
    wldcore_map_visibility_level_t visibility;
    wldcore_map_path_level_t path;
    wldcore_menu_location_menu_level_t location_menu;
    wldcore_location_list_level_t location_list;
    wldcore_proposition_report_level_t proposition_report;
    wldcore_menu_sound_novel_level_t sound_novel;
    wldcore_proposition_result_level_t proposition_result;
    wldcore_bar_active_propositions_level_t active_propositions;
    u8 raw[0x5c];
} wldcore_menu_stack_record_t;

typedef char wldcore_menu_stack_record_size_must_be_0x5c[sizeof(wldcore_menu_stack_record_t) == 0x5c ? 1 : -1];

/* Provisional: owner object passed to 0x80090a28; only the window record
 * index at +0x38 and the compared words at +0x50/+0x54 are established. Its
 * only caller passes a cast wldcore_text_scrollable_window_t, whose fields at
 * those offsets are left_window_index, last_page_row and page_start_row. */
typedef struct wldcore_window_owner {
    u8 unknown_00[0x38];
    s32 window_index; /* 0x38 */
    u8 unknown_3c[0x14];
    s32 last_page_row;  /* 0x50 */
    s32 page_start_row; /* 0x54; flag 0x10 cleared while page_start_row < last_page_row */
} wldcore_window_owner_t;

typedef struct wldcore_point32 {
    s32 x;
    s32 y;
} wldcore_point32_t;

/* The 52-byte world map dot records at 0x800d3ca8. */
typedef struct wldcore_map_dot {
    s32 kind;
    s32 sub_kind;
    SVECTOR position; /* 0x08; world-map position, RotTrans input */
    s32 flags;
    s32 sprite_id;
    s32 priority;     /* 0x18; highest wins in wldcore_map_find_dot_at_point */
    s32 frame_index;  /* 0x1c; display object (at &flags) frame_index, cleared on sprite changes */
    s32 anim_counter; /* 0x20; display object anim_counter, cleared with frame_index */
    /* 0x24; CLUT row offset read as wldcore_anim_object_t.palette (0 keeps the
     * sprite's own CLUT). The player's marker (0x8009f278) takes 0, 2 or 3 from
     * EVENT_SCRIPT_VAR_TOWN_BACKGROUND, Ramza's chapter palette. */
    s32 palette;
    s32 screen_x;
    s32 screen_y;
    u8 rgb[3];
    u8 pad2;
} wldcore_map_dot_t;

typedef char wldcore_map_dot_size_must_be_0x34[sizeof(wldcore_map_dot_t) == 0x34 ? 1 : -1];

/* The world map camera projection state at 0x8009f24c: two control words
 * followed by the player's own map dot. The marker's +8 base is what makes
 * the views of this address agree field for field -- the camera update
 * writes the dot's sprite_id and screen_x/screen_y and the marker init writes
 * its kind, position and flags. For the player's marker, kind is the current
 * location id and sub_kind the proposition picture id. */
typedef struct wldcore_projection_state {
    s32 flags;
    s32 angle;
    wldcore_map_dot_t marker;
} wldcore_projection_state_t;

/* The volatile first store preserves the target's R3000 load-delay slot. */
typedef struct wldcore_map_clamped_point32 {
    volatile s32 x;
    s32 y;
} wldcore_map_clamped_point32_t;

/* The camera's in-progress wrapped projection. The three map-step routines
 * agree on this complete 0x24-byte record; flags is the active bitfield. */
typedef struct wldcore_projection_motion {
    s32 flags;
    s32 distance;
    s32 progress;
    VECTOR origin;
    wldcore_map_clamped_point32_t target;
} wldcore_projection_motion_t;
typedef char wldcore_projection_motion_size_must_be_0x24[sizeof(wldcore_projection_motion_t) == 0x24 ? 1 : -1];

/* Two unsigned coordinates passed together in one argument register. */
typedef struct wldcore_xy16 {
    u16 x;
    u16 y;
} wldcore_xy16_t;

/* The three parallel arrays share one object, which is why the target builds
 * all three walk pointers from a single symbol address. */
typedef struct wldcore_sortbox_state {
    u32 flags[2];
    wldcore_xy16_t priorities[2]; /* 0x08; .x is the GsSortBoxFill OT priority of each box */
    s32 elapsed;                  /* 0x10; fade frames run, stepped by wldcore_fade_step_screen */
    s32 duration;                 /* 0x14 */
    GsBOXF boxes[2];              /* 0x18; full-screen fill boxes drawn by wldcore_fade_draw_screen_overlays */
} wldcore_sortbox_state_t;

typedef char wldcore_sortbox_state_size_must_be_0x38[sizeof(wldcore_sortbox_state_t) == 0x38 ? 1 : -1];

typedef char wldcore_xy16_size_must_be_4[sizeof(wldcore_xy16_t) == 4 ? 1 : -1];

typedef struct wldcore_window_render_bounds16 {
    wldcore_xy16_t position;
    wldcore_xy16_t dimensions;
} wldcore_window_render_bounds16_t;

/* Verified fields of the 52-byte render records at 0x800bbc70. The pool init
 * 0x8006ad28 sets flags = 2, so the display-object dispatcher draws them as
 * sprites through wldcore_gfx_draw_display_object_sprite
 * (wldcore_display_object_t). */
typedef struct wldcore_window_render_record {
    s32 flags;
    s32 tpage;
    s32 priority; /* 0x08; OT priority passed to the sprite sort; 1 from the pool init */
    s32 coordinate_mode;
    s32 anim_counter; /* 0x10; flag-0x100 open-animation counter, cleared on every rebind */
    /* 0x14; CLUT row offset: nonzero gives cx = clut_x + (palette - 1) << 4. The window-state
     * routines at 0x80076744/0x8007688c clear it or set it to 2. */
    s32 palette;
    s32 base_x;
    s32 base_y;
    s32 clut_x; /* 0x20; 0x3c0 from the pool init */
    s32 clut_y; /* 0x24; 0x1f0 from the pool init */
    u16 x;
    u16 y;
    s16 width;
    u16 height;
    u8 red;
    u8 green;
    u8 blue;
    u8 unknown_33;
} wldcore_window_render_record_t;

typedef char wldcore_render_record_size_must_be_0x34[sizeof(wldcore_window_render_record_t) == 0x34 ? 1 : -1];

/* State for a paged text window and its four decorative window records. */
typedef struct wldcore_text_scrollable_window {
    s32 image_x; /* 0x00; text image position.x for wldcore_window_build_render_record_image; cleared by the setups */
    s32 image_y; /* 0x04; text image position.y; 0x70 or 0x60 from the setups */
    wldcore_point32_t base;
    s32 text_width;
    s32 rows_per_page;
    s32 measured_columns;
    s32 measured_rows;
    /* 0x20..0x2e: copy of the optional extra render record's base_x/base_y and x/y/width/height, taken
     * as two block copies; 0x800906c0 scrolls and clips the record from it. */
    s32 extra_base_x;
    s32 extra_base_y; /* 0x24; scrolled by row * 16 into the record's base_y */
    u16 extra_x;
    u16 extra_y; /* 0x2a; written (plus the clipped offset) to the record's y */
    u16 extra_width;
    s16 extra_height; /* 0x2e; written (minus the clipped offset) to the record's height */
    /* 0x30; wldcore_text_is_window_finished: 0 paging, 1 manual scroll while button 0x80 is held, 2 scrolling
     * back to page_start_row */
    s32 scroll_state;
    s32 text_id;
    s32 left_window_index;
    s32 middle_window_index;
    s32 right_window_index;
    s32 render_record_index;
    s32 lower_window_index;
    s32 priority; /* 0x4c; OT priority of the text window's records (wldcore_window_init_record) */
    s32 last_page_row;
    s32 page_start_row;
    s32 scroll_row;            /* 0x58; row shown while 0x800903e4 scrolls (scroll_state 1/2) */
    s32 image_coordinate_mode; /* 0x5c; coordinate mode of the text render record image */
    s32 scroll_timer;          /* 0x60; frame counter of the auto-scroll state */
    s32 extra_render_index;    /* 0x64; optional render record scrolled with the text, -1 if none */
    s32 text_substitutions[2]; /* 0x68; copied to the text substitution values unless -1 */
} wldcore_text_scrollable_window_t;

typedef char
    wldcore_scrollable_text_window_size_must_be_0x70[sizeof(wldcore_text_scrollable_window_t) == 0x70 ? 1 : -1];

/* Provisional: stride-only views of 36- and 52-byte record arrays, for a
 * column that must be indexed as its own array. Field access on
 * g_wldcore_window_records/g_wldcore_window_render_records emits the same ASPSX $at
 * form, so prefer the record type where the column's type agrees. */
typedef struct wldcore_window_entry_36 {
    s32 value;
    s8 rest[32];
} wldcore_window_entry_36_t;

typedef char wldcore_entry_36_size_must_be_0x24[sizeof(wldcore_window_entry_36_t) == 0x24 ? 1 : -1];

typedef struct wldcore_window_entry_52 {
    s32 value; /* x position for the 0x800d3cd0 column */
    s32 y;     /* second position word, read by 0x8006d928 */
    s8 rest[44];
} wldcore_window_entry_52_t;

typedef char wldcore_entry_52_size_must_be_0x34[sizeof(wldcore_window_entry_52_t) == 0x34 ? 1 : -1];

/* Halfword column of the 52-byte records (width at +0x2c, height at +0x2e). */
typedef struct wldcore_window_entry_52_s16 {
    s16 value;
    u8 rest[50];
} wldcore_window_entry_52_s16_t;

typedef char wldcore_entry_52_s16_size_must_be_0x34[sizeof(wldcore_window_entry_52_s16_t) == 0x34 ? 1 : -1];

/* Colour triple of the 36-byte window records (+0x20), which the header
 * record spells as one word. */
typedef struct wldcore_window_entry_36_rgb {
    u8 red;
    u8 green;
    u8 blue;
    u8 rest[33];
} wldcore_window_entry_36_rgb_t;

typedef char wldcore_entry_36_rgb_size_must_be_0x24[sizeof(wldcore_window_entry_36_rgb_t) == 0x24 ? 1 : -1];

/* Colour triple of the 52-byte render records (+0x30). */
typedef struct wldcore_window_entry_52_rgb {
    u8 red;
    u8 green;
    u8 blue;
    u8 rest[49];
} wldcore_window_entry_52_rgb_t;

typedef char wldcore_entry_52_rgb_size_must_be_0x34[sizeof(wldcore_window_entry_52_rgb_t) == 0x34 ? 1 : -1];

/* Index descriptor passed to the paired window-state routines 0x80076744
 * and 0x8007688c; each word selects a window or render record. */
typedef struct wldcore_window_tint_request {
    s32 window_a;
    s32 render_a;
    u8 unknown_08[8];
    s32 render_color; /* 0x10; render record whose colour is set */
    s32 render_b;
    s32 window_b;
    u8 unknown_1c[0x24];
    s32 window_c; /* 0x40; cleared only by 0x80076744 */
    s32 window_d;
} wldcore_window_tint_request_t;

/* libgs GsOT descriptor: the 20-byte ordering-table records at 0x800bb364
 * (main) and 0x800bb3c4 (auxiliary), one per display buffer. The 0x14 stride
 * is what wldcore_init_core indexes these globals with. */
/* The ordering-table handle is libgs GsOT; see include/psx/gs.h. */

/* The 8-byte texture rectangle of a display object, and the same shape again
 * as the frame descriptor 0x8006b678 fills in. */
typedef struct wldcore_display_rect {
    u16 u; /* 0x00 */
    u16 v; /* 0x02 */
    u16 w; /* 0x04 */
    u16 h; /* 0x06 */
} wldcore_display_rect_t;

/* Display-object queue entry dispatched by wldcore_dispatch_display_object_list.
 * The leading flags word selects the handler; the remaining fields are the
 * view of the sprite handler wldcore_gfx_draw_display_object_sprite. */
typedef struct wldcore_display_object {
    s32 flags;                   /* 0x00 */
    s32 tpage;                   /* 0x04 */
    u16 priority;                /* 0x08 */
    u16 unknown_0a;              /* 0x0A */
    u8 unknown_0c[4];            /* 0x0C */
    s32 anim_counter;            /* 0x10 */
    s32 palette;                 /* 0x14; CLUT row offset: nonzero gives sprite cx = cx + (palette - 1) << 4 */
    s32 x;                       /* 0x18 */
    s32 y;                       /* 0x1C */
    s32 cx;                      /* 0x20 */
    s32 cy;                      /* 0x24 */
    wldcore_display_rect_t rect; /* 0x28 */
    CVECTOR color;               /* 0x30 */
} wldcore_display_object_t;

typedef char wldcore_display_object_size_must_be_0x34[sizeof(wldcore_display_object_t) == 0x34 ? 1 : -1];

/* One 23-byte proposition row expanded to halfword fields by
 * wldcore_unpack_proposition_row; `fields` names the halfwords whose use is
 * known. */
typedef union wldcore_proposition_fields {
    u16 values[23];
    struct {
        u16 category_index; /* 0x00; 1-based row in proposition data records 3, 10 and 11 */
        u16 job_list_index; /* 0x02 */
        u16 id;             /* 0x04 */
        u16 field_06;       /* 0x06 */
        u16 fee_a;          /* 0x08; remapped through data record 1 */
        u16 fee_b;          /* 0x0a; remapped through data record 1 */
        u16 min_days;       /* 0x0c */
        u16 max_days;       /* 0x0e */
        u16 location;       /* 0x10 */
        u16 jp_reward;      /* 0x12 */
        u16 discovery_kind; /* 0x14; 1 treasure, 2 unexplored land, else see random_bonus */
        u16 random_bonus;   /* 0x16; 1 enables the three-tier random reward */
        u16 gil_reward;     /* 0x18 */
        u16 field_1a[6];    /* 0x1a */
        u16 speaker_mode;   /* 0x26 */
        u16 preferred_job;  /* 0x28 */
        u16 field_2a[2];    /* 0x2a */
    } fields;
} wldcore_proposition_fields_t;

typedef char wldcore_proposition_fields_size_must_be_0x2e[sizeof(wldcore_proposition_fields_t) == 0x2e ? 1 : -1];

/* Provisional: one axis of the accelerating map scroll stepped by
 * wldcore_map_update_scroll_axis_speed: the fixed-point accumulator, its
 * signed limit, the acceleration and decay steps, and the exported whole-unit
 * speed. */
typedef struct wldcore_axis_state {
    s32 value;
    s32 limit;
    s32 step;
    s32 decay;
    s32 output; /* 0x10; value >> 8, truncated toward zero */
} wldcore_axis_state_t;

/* Provisional: an adjacent word pair copied as one structure. The paired
 * type reproduces the target's two-load/two-store schedule. */
typedef struct wldcore_word_pair {
    s32 first;
    s32 second;
} wldcore_word_pair_t;

/* Provisional: the column view of the 36-byte window records rebased on the
 * x/y pair wldcore_window_record_t places at +0x18. */
typedef struct wldcore_window_xy {
    s32 x;
    s32 y;
    s8 rest[28];
} wldcore_window_xy_t;

typedef char wldcore_window_xy_size_must_be_0x24[sizeof(wldcore_window_xy_t) == 0x24 ? 1 : -1];

/* Provisional: the save-slot word list at g_wldcore_script_slot_table: section offsets into
 * the save-slot buffer, indexed from +0x04. */
typedef struct wldcore_save_slot_table {
    s32 unknown_00;
    u32 offsets[1]; /* 0x04 */
} wldcore_save_slot_table_t;

/* Provisional: four-byte per-location records reached through g_wldcore_location_records. */
typedef struct wldcore_state_record {
    u8 unknown_00[2];
    u8 picture; /* 0x02; proposition picture + 1, 0 for none */
    u8 is_town; /* 0x03; 1 offers the fixed town entries 0xb85d-0xb85f in the location menu */
} wldcore_state_record_t;

/* Provisional: the slideshow page list at 0x800d3bbc. A route travel reuses
 * the record: the route id is the script variable (offset by 0x22c, the
 * route-visible variables), each page is one two-point segment of the route
 * polyline that follows the page list, shown for its length / 32 frames, and
 * page i runs from point 2i to point 2i + 2. */
typedef struct wldcore_slideshow {
    u16 flags;           /* 0x00; bit 0 runs the page timer, bit 1 = reversed */
    s16 script_variable; /* 0x02; script variable base, offset by 0x22c */
    s16 page_count;      /* 0x04 */
    s16 page_index;      /* 0x06 */
    s16 page_frames[16]; /* 0x08; frames each page stays up */
    u16 frame_timer;     /* 0x28 */
    s16 point_count;     /* 0x2a */
    s16 coords[1];       /* 0x2c; packed (x, y) pairs */
} wldcore_slideshow_t;

/* Animated display object drawn by wldcore_gfx_draw_animated_display_object
 * (0x8006aed0), which reads priority as a halfword. */
typedef struct wldcore_anim_object {
    s32 flags;        /* 0x00 */
    s32 sequence;     /* 0x04; row of the sequence table at g_wldcore_anim_sequence_table */
    u16 priority;     /* 0x08 */
    u16 unknown_0a;   /* 0x0A */
    s32 frame_index;  /* 0x0C */
    s32 anim_counter; /* 0x10 */
    s32 palette;      /* 0x14 */
    s32 x;            /* 0x18 */
    s32 y;            /* 0x1C */
    CVECTOR color;    /* 0x20 */
} wldcore_anim_object_t;

typedef char wldcore_anim_object_size_must_be_0x24[sizeof(wldcore_anim_object_t) == 0x24 ? 1 : -1];

/* Provisional: 0x28-byte request block handed to 0x8006aed0. Its builders
 * store 0x08 as one word, so they keep this view and pass it as a
 * wldcore_anim_object_t; the leading words follow that layout. */
typedef struct wldcore_anim_draw_request {
    s32 flags;        /* 0x00; 1, the animated-object flag */
    s32 sequence;     /* 0x04; e.g. month + 0x1f for the calendar */
    s32 priority;     /* 0x08; 4 */
    s32 frame_index;  /* 0x0c; cleared before every draw */
    s32 anim_counter; /* 0x10; cleared before every draw */
    s32 palette;      /* 0x14 */
    s32 x;            /* 0x18; screen x, stepped per glyph */
    s32 y;            /* 0x1c */
    CVECTOR color;
    u8 unknown_24[4];
} wldcore_anim_draw_request_t;
typedef char wldcore_anim_draw_request_size_must_be_0x28[sizeof(wldcore_anim_draw_request_t) == 0x28 ? 1 : -1];

/* Provisional: proposition-resolution work record at 0x8009f1ec. One object:
 * wldcore_proposition_find_preferred_job_unit reads excluded_party_index at
 * -0xc from its &proposition_index base, and calculate_base_jp_and_gil and
 * step_result_level reach -0x10..+0x24 from single bases. */
typedef struct wldcore_job_selection {
    s32 excluded_party_index; /* 0x00 */
    s32 speaker_party_index;  /* 0x04; chosen and read back by wldcore_proposition_step_participant_message */
    s32 preferred_unit;       /* 0x08; fixed speaker passed to wldcore_proposition_step_participant_message */
    s32 proposition_index;    /* 0x0c; index into g_main_active_propositions */
    s32 result;               /* 0x10; 0 success, 1 and 2 failure kinds */
    s32 reward_type;          /* 0x14 */
    s32 reward_index;         /* 0x18 */
    s32 reward_value;         /* 0x1c */
    s32 gate;                 /* 0x20; set for a dispatched proposition */
    u8 unknown_24[0x10];
    s32 rows[3][3]; /* 0x34; per-participant score/report rows */
} wldcore_job_selection_t;

typedef char wldcore_job_selection_size_must_be_0x58[sizeof(wldcore_job_selection_t) == 0x58 ? 1 : -1];

/* The music words at +0x8c: the first is both the music flags and slot 0 of
 * the per-slot loaded-track table, since the target indexes the table from
 * that address. */
typedef union wldcore_audio_music_slots {
    s32 flags;
    s32 tracks[3];
} wldcore_audio_music_slots_t;

/* Audio command FIFO at 0x800d45a4, filled by
 * wldcore_sound_enqueue_audio_command and serviced by wldcore_sound_process_audio_queue, followed
 * by the music state that consumer owns. current_command and count are the
 * busy words the queue-drain waits test. */
typedef struct wldcore_audio_queue {
    s32 commands[16];
    s32 values[16];
    s32 current_command;               /* 0x80 */
    s32 current_value;                 /* 0x84 */
    s32 count;                         /* 0x88 */
    wldcore_audio_music_slots_t music; /* 0x8c */
    s32 current_slot;                  /* 0x98 */
    s32 volume;                        /* 0x9c */
} wldcore_audio_queue_t;

typedef char wldcore_audio_queue_size_must_be_0xa0[sizeof(wldcore_audio_queue_t) == 0xa0 ? 1 : -1];

/* Provisional: a 16-entry world-map route path, each entry a route id with
 * bit 8 set when the route is travelled end-to-start. */
typedef struct wldcore_route_path {
    s32 routes[16];
} wldcore_route_path_t;

/* Provisional: world-map route record header: the segment count, the two
 * endpoint location ids and the travel cost. The 12-byte route points follow
 * at +4. */
typedef struct wldcore_route_record {
    u8 count;
    u8 start_location; /* 0x01; a forward route runs start_location -> end_location */
    u8 end_location;   /* 0x02; route | 0x100 travels it end to start */
    u8 cost;           /* 0x03; accumulated by wldcore_map_find_best_route_path */
} wldcore_route_record_t;

/* Provisional: location-entry state at 0x800d0b24, filled by the route search
 * wldcore_map_find_best_route_path, staged per route leg by wldcore_location_begin_route_segment and stepped by
 * wldcore_map_step_route_travel. The vertices are SVECTORs: their 2-byte alignment is what
 * makes `previous_position = position` take GCC's unaligned block-move path
 * (lwl/lwr, swl/swr) that the target emits. */
typedef struct wldcore_location_entry_state {
    s32 flags;                       /* 0x00; bit 0 entry found, bit 1 descending leg */
    s32 route_id;                    /* 0x04; route & 0xff, index into g_wldcore_map_route_tables */
    s32 segment_index;               /* 0x08; current segment of the leg, the step for begin_route_segment */
    s32 segment_count;               /* 0x0c; wldcore_location_begin_route_segment result (record count - 1) */
    s32 segment_frame;               /* 0x10; frames into the segment, interpolation numerator */
    s32 segment_frames;              /* 0x14; point distance * 2, interpolation denominator */
    wldcore_route_path_t route_path; /* 0x18; best path found */
    s32 route_index;                 /* 0x58; route_path.routes index of the current leg */
    s32 best_path_length;            /* 0x5c; g_wldcore_route_best_path_length */
    s32 best_cost;                   /* 0x60; g_wldcore_route_best_cost, reset to 99999 */
    s32 best_length_bound;           /* 0x64; g_wldcore_route_best_length_bound, reset to 99999 */
    s32 leg_start_location;          /* 0x68; route record start (end when reversed) */
    s32 leg_end_location;            /* 0x6c; becomes the current location and script variable 0x31 on arrival */
    s32 heading;                     /* 0x70; route point heading (+0x800 when reversed), copied to the marker angle */
    s32 menu_level;                  /* 0x74; parent menu depth */
    SVECTOR previous_position;       /* 0x78 */
    SVECTOR position;                /* 0x80 */
    SVECTOR target_position;         /* 0x88 */
    s32 arrival_location;            /* 0x90; arrival location id + 1 */
} wldcore_location_entry_state_t;

typedef char wldcore_location_entry_state_size_must_be_0x94[sizeof(wldcore_location_entry_state_t) == 0x94 ? 1 : -1];

/* Provisional: view of g_wldcore_map_projection_state at 0x8009f24c used by
 * the location-entry routines. Like wldcore_map_dot_t it holds the marker's
 * position as one SVECTOR, which the route interpolation assigns whole (a block
 * move); it also spells screen_x/screen_y as one point passed by address, which
 * wldcore_map_dot_t keeps as separate scalars. */
typedef struct wldcore_location_view {
    s32 projection_state; /* 0x00; g_wldcore_map_projection_state.flags */
    s32 angle;            /* 0x04; .angle */
    s32 location_id;      /* 0x08; .marker.kind */
    s32 picture_id;       /* 0x0c; .marker.sub_kind */
    SVECTOR coordinates;  /* 0x10; marker x/y/z */
    u8 unknown_18[0x18];
    wldcore_point32_t point; /* 0x30; marker screen_x/screen_y */
} wldcore_location_view_t;

typedef char wldcore_location_view_size_must_be_0x38[sizeof(wldcore_location_view_t) == 0x38 ? 1 : -1];

/* Provisional: 0x14-byte parameter blocks at 0x8009ebc0 for WORLD threads 8,
 * 12 and 9; each thread receives its block's address as its first
 * parameter. */
typedef struct wldcore_thread_block {
    s32 x; /* 0x00; panel draw x (thread 9 draws at x - 0x80); WORLD twin world_status_thread_t.x */
    s32 y; /* 0x04; added to the panel's draw offset; thread 8's shake offset */
    u8 unknown_08[4];
    s32 redraw_request; /* 0x0c; scroll direction for thread 8, redraw request for 12 and 9 */
    u8 unknown_10[4];
} wldcore_thread_block_t;

typedef char wldcore_thread_block_size_must_be_0x14[sizeof(wldcore_thread_block_t) == 0x14 ? 1 : -1];

/* Provisional: the 24-byte projected-tile table at 0x800c7320, filled by
 * wldcore_map_init_tile_atlas and projected by the RotTrans pass. */
typedef struct wldcore_projected_entry {
    s32 flags;    /* 0x00; texture page */
    s32 texture;  /* 0x04; texture rectangle packed u << 24 | v << 16 | w << 8 | h, unpacked into the poly UVs */
    s32 map_x;    /* 0x08; tile x (-256 + widths), RotTrans vx */
    s32 map_y;    /* 0x0c; tile y (-192 + heights), RotTrans vy */
    s32 screen_x; /* 0x10; projected x, culled against the 0x121 screen span */
    s32 screen_y; /* 0x14; projected y, culled against 0x111 */
} wldcore_projected_entry_t;

typedef char wldcore_projected_entry_size_must_be_0x18[sizeof(wldcore_projected_entry_t) == 0x18 ? 1 : -1];

extern s32 g_wldcore_map_dot_count;
extern wldcore_map_dot_t g_wldcore_map_dots[];
extern u32 g_wldcore_previous_system_flags;
extern s32* g_wldcore_proposition_data_base;
extern u32 g_wldcore_current_button_input;
extern u8 g_wldcore_days_per_month[12]; /* 0x8009e66c; non-leap-year month lengths */
extern u32 g_wldcore_gfx_aux_ot_tags[2][4];
extern GsOT g_wldcore_gfx_aux_ordering_tables[2];
extern u32 g_wldcore_gfx_ot_tags[2][16];
extern GsOT g_wldcore_gfx_ordering_tables[2];
extern u8 g_wldcore_gfx_world_primitive_buffers;
/* Per-row flag column of the world list panels; the list steps compare it
 * signed, and the two writers that only store into it agree on the width. */
extern s16 g_wldcore_list_row_flags[];
extern wldcore_projection_state_t g_wldcore_map_projection_state;
extern wldcore_projection_motion_t g_wldcore_map_projection_motion;
extern wldcore_job_selection_t g_wldcore_job_selection;
extern wldcore_location_entry_state_t g_wldcore_location_entry_state;
extern wldcore_projected_entry_t g_wldcore_map_projected_tiles[];
/* Pointer to the 48 route polylines (array at 0x80095fb0), one per route;
 * each is a packed s16 count followed by that many (x, y) pairs (0x80096070
 * holds count 12 and the next record starts at 0x800960a2). A sibling of
 * g_wldcore_map_route_tables, indexed by the same route id. */
extern u16** g_wldcore_map_route_polylines;
/* Pointer to the array of 48 route-record pointers at 0x80095000. */
extern wldcore_route_record_t** g_wldcore_map_route_tables;
extern wldcore_thread_block_t g_wldcore_thread8_params;
extern wldcore_thread_block_t g_wldcore_thread9_params;
extern wldcore_thread_block_t g_wldcore_threadc_params;
extern wldcore_menu_cursor_state_t g_wldcore_menu_cursor_states[];
/* libgs sprite sorted by wldcore_draw_close_indicator_if_pending through
 * world_gs_sortsprite (0x800e0480). */
extern GsSPRITE g_wldcore_menu_indicator_sprite;
extern s32 g_wldcore_menu_result;
extern s32 g_wldcore_menu_stack_depth;
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records[];
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records_next[];
extern s32 g_wldcore_menu_stack_types[];
extern s32 g_wldcore_menu_stack_types_next[];
extern u32 g_wldcore_new_button_presses;
extern wldcore_opcode_state_t g_wldcore_opcode_state;
extern u32 g_wldcore_polled_button_input;
extern u32 g_wldcore_previous_button_input;
extern s32 g_wldcore_proposition_selected_entry;
extern s32 g_wldcore_proposition_send_unit_count;
/* Staged world-script arguments at 0x800d4664 (event ids, SetVar2 and path
 * arguments, deep-dungeon entry values). */
extern u8 g_wldcore_sound_quad_command_bytes[][4];
/* Scalar view of g_wldcore_active_saved_record.state_flags for opcode
 * handlers that read-modify-write it beside another record store: the member
 * spelling shares one base register where the target uses absolute addresses. */
extern u16 g_wldcore_state_flags;
extern s32 g_wldcore_window_aux_render_object_count;
extern u32* g_wldcore_window_aux_render_object_queue[];
/* Shared panel renderer state at 0x800bb31c, filled by
 * wldcore_window_init_panel_render_state and
 * wldcore_window_init_number_panel_render_thread and read by
 * world_build_at_list_2. Its text binding is the label layout at 0x800bb394
 * (column mode 0 text, 2 hidden, 3 alternate number) or, for number panels,
 * g_wldcore_window_panel_number_state. */
extern world_menu_text_layout_t g_wldcore_window_panel_label_state;
extern world_menu_entry_t g_wldcore_window_panel_render_state;
extern s32 g_wldcore_window_record_count;
extern wldcore_window_record_t g_wldcore_window_records[];
extern wldcore_window_layout_row_t g_wldcore_window_layout_rows[];
extern s32 g_wldcore_window_render_object_count;

/* Display-object queues drawn by 0x8006ae20 each frame: every entry points at
 * the leading flags word of a window record, render record or map dot, and
 * wldcore_window_set_render_objects_visible toggles bit 0x08 through it. */
extern u32* g_wldcore_window_render_object_queue[];
extern wldcore_window_render_record_t g_wldcore_window_render_records[];

/* Unnamed data, in address order. */
extern wldcore_word_pair_t g_main_saved_list_cursor_state;
extern wldcore_proposition_progress_t g_main_active_propositions[8];
extern wldcore_saved_record_t g_main_saved_records[];
extern u32 g_main_secondary_saved_data_bits[2];
extern wldcore_state_record_t* g_wldcore_location_records;
extern s32* g_wldcore_rumor_location_masks;
extern s32 g_wldcore_picture_sector_offsets[];
extern s32 g_wldcore_background_sector_offsets[];
extern s32 g_wldcore_message_block_sector_offsets[];
extern s32 g_wldcore_sound_novel_script_sector_offsets[];
extern s32 g_wldcore_sound_novel_text_sector_offsets[];
extern s32 g_wldcore_thread8_offset_y;
extern s16 g_wldcore_panel_row_actions[16];
extern s32 g_wldcore_location_marker_shade;
extern wldcore_text_scrollable_window_t g_wldcore_scrollable_text_window;
extern s32 g_wldcore_scroll_text_render_record_index;
/* The horizontal and vertical map scroll axes. */
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_x;
extern wldcore_axis_state_t g_wldcore_map_cursor_scroll_y;
/* Sound resource the sound-novel menu passes to SuzukiAppendVFXSMD. */
extern struct main_sound_resource* g_wldcore_sound_novel_sound_resource;
extern s32 g_wldcore_window_render_record_count;
extern wldcore_location_view_t g_wldcore_location_view;
/* Colour of the context-value display, tinted by wldcore_gfx_copy_color_with_tint. */
extern CVECTOR g_wldcore_hud_color;
/* Screen position of the context-value display; the sound-novel menu saves
 * and restores both words as one wldcore_point32_t. */
extern wldcore_point32_t g_wldcore_context_value_display_position;
extern u8 g_wldcore_map_dot_pulse_direction;
extern u8 g_wldcore_map_dot_pulse_phase;
extern u8 g_wldcore_map_projection_rotation_bytes[];
extern wldcore_proposition_fields_t g_wldcore_selected_proposition_row[];
/* HUD ordering-table priority. The sort calls read it as a halfword (lhu);
 * its three setters store a whole word. */
extern u16 g_wldcore_hud_ot_priority;
extern s32 g_wldcore_sound_novel_countdown_frames;
extern s16 g_wldcore_saved_record_index;
extern s32 g_wldcore_loaded_background_set;
extern wldcore_window_xy_t g_wldcore_window_record_positions[];

/* Width/height columns of g_wldcore_window_render_records. The text-window routines
 * at 0x80075950 and 0x8008aea0 read the height signed while
 * wldcore_window_load_image_record_to_vram reads it unsigned, so they keep these
 * signed views. */
extern wldcore_window_entry_52_s16_t g_wldcore_window_render_record_widths[];
extern wldcore_window_entry_52_s16_t g_wldcore_window_render_record_heights[];
extern u8 g_wldcore_message_buffer[];
extern u32 g_wldcore_window_image_buffer[];
/* Row value column of the world list panels, read signed as `value & 0x7ff`. */
extern s16 g_wldcore_list_entry_values[];
extern wldcore_word_pair_t g_wldcore_story_events_cursor_state;
extern wldcore_word_pair_t g_wldcore_tutorial_entries_cursor_state;
extern s32 g_wldcore_proposition_gil_amount;
extern s32 g_wldcore_context_value_display_mode;
extern s32 g_wldcore_sound_novel_countdown_section;
extern s32 g_wldcore_sound_novel_countdown_elapsed;
extern s32 g_wldcore_proposition_send_units[];
extern s32 g_wldcore_displayed_numeric_value;
extern s32 g_wldcore_displayed_numeric_value_secondary;
extern s32 g_wldcore_displayed_numeric_value_tertiary;
extern s16 g_wldcore_proposition_candidate_levels[];
/* Currently resident picture set, reset to -1 by wldcore_init_world_map_state. */
extern s32 g_wldcore_loaded_picture_set;
extern s32 g_wldcore_loaded_message_block;
extern s32 g_wldcore_route_search_depth;
extern wldcore_slideshow_t g_wldcore_map_path_animation;
extern wldcore_window_entry_52_t g_wldcore_map_dot_screen_y[];
extern u32 g_wldcore_sound_novel_local_flags[];
extern s32 g_wldcore_sound_novel_wait_counter;
extern s32 g_wldcore_proposition_dispatch_days;
extern wldcore_audio_queue_t g_wldcore_audio_queue;
extern s32 g_wldcore_route_search_cost;
extern wldcore_save_slot_table_t* g_wldcore_script_slot_table;
/* The branch opcodes subtract this halfword sign-extended, so it is signed;
 * the three flag-0x04 handlers reach the same storage through a u16 view. */
/* Unsigned views of g_wldcore_active_saved_record.text_history_0..3 for the
 * two history shifters, which load them with lhu; the record keeps s16 for
 * the -1 stores of wldcore_reset_saved_record_fields. */
extern u16 g_wldcore_sound_novel_text_history_0;
extern u16 g_wldcore_sound_novel_text_history_1;
extern u16 g_wldcore_sound_novel_text_history_2;
extern u16 g_wldcore_sound_novel_text_history_3;
extern s32 g_wldcore_map_window_index;
extern s32 g_wldcore_sound_novel_wait_frames;
/* Saved copy of g_wldcore_context_value_display_position. */
extern wldcore_point32_t g_wldcore_saved_context_value_display_position;
extern void* g_world_text_message_section;
extern s16 g_wldcore_active_menu_value;
/* Saved proposition-list cursor row. Declared as an array: GCC's alias
 * heuristic needs an in-struct object to keep the target's load order in
 * wldcore_list_handle_active_propositions_input; a scalar does not. */
extern s32 g_wldcore_active_propositions_saved_cursor[];
extern wldcore_saved_record_t g_wldcore_active_saved_record;
extern u32** g_wldcore_anim_cel_table;
extern s32 g_wldcore_bar_menu_saved_cursor;
extern u8 g_wldcore_bar_row_help_text_ids[];
extern u8 (*g_wldcore_brave_story_birthdays)[2];
extern u8 g_wldcore_brave_story_help_text_ids[];
extern s32 g_wldcore_brave_story_saved_cursor[];
extern u8 g_wldcore_choice_hidden_variable_offsets[];
/* Music/sound selection halfword at 0x800d486a: track in the low byte, slot
 * in the high byte. */
extern s32 g_wldcore_date_display_x;
extern s32 g_wldcore_date_display_y;
extern u16* g_wldcore_dissolve_band_order;
extern u8 g_wldcore_dissolve_band_progress[16];
extern s32 g_wldcore_input_repeat_counter_up;
extern s32 g_wldcore_input_repeat_counter_down;
extern s32 g_wldcore_input_repeat_counter_left;
extern s32 g_wldcore_input_repeat_counter_right;
extern s32 g_wldcore_input_repeat_counter_r1;
extern s32 g_wldcore_input_repeat_counter_l1;
extern s32 g_wldcore_input_repeat_initial_delay;
extern s32 g_wldcore_input_repeat_period;
extern s32 g_wldcore_input_secondary_repeat_period;
extern u32* g_wldcore_location_frame_tim;
extern wldcore_window_entry_52_t g_wldcore_map_dot_screen_x[];
extern VECTOR g_wldcore_map_projection_origin;
extern SVECTOR g_wldcore_map_projection_rotation;
extern VECTOR g_wldcore_map_projection_scale;
extern s32* g_wldcore_map_tile_descriptors;
/* Provisional: the zoom animation record at 0x800d3c8c; flags bit 0 runs
 * the animation, bit 1 selects the zoomed-out direction. */
extern wldcore_projection_motion_t g_wldcore_map_zoom_motion;
extern void (*g_wldcore_menu_idle_handlers[])(wldcore_menu_stack_record_t*);
extern s32 g_wldcore_menu_ordering_table_offset;
extern void (*g_wldcore_menu_publish_handlers[])(wldcore_menu_stack_record_t*);
extern void (*g_wldcore_menu_resume_handlers[])(wldcore_menu_stack_record_t*);
/* Indexed so the store in wldcore_location_process_entry stays behind the
 * window-flag store (MEM_IN_STRUCT_P). */
extern s32 g_wldcore_next_map_id[];
extern void (*g_wldcore_opcode_handlers[])(void);
extern s32 g_wldcore_participant_list_saved_cursor;
/* Destination image buffer (read back as a TIM by wldcore_opcode_load_picture). */
extern u32* g_wldcore_picture_buffer;
/* Saved brave-story row, declared as an array for the same reason as
 * g_wldcore_active_propositions_saved_cursor. */
extern s32 g_wldcore_proposition_category_saved_cursor[];
extern u32 g_wldcore_proposition_detail_image_buffer[];
extern s32 g_wldcore_random_battle_entd_id;
extern s32 g_wldcore_random_battle_map_id;
extern s32 g_wldcore_random_battle_squad_id;
extern u32 g_wldcore_saved_system_flags;
extern s32* g_wldcore_scratch_buffer;
/* Per-channel tint factors, one byte each, 0x80 == unity. */
extern u8 g_wldcore_screen_brightness_rgb[3];
extern wldcore_sortbox_state_t g_wldcore_screen_fade_state;
/* Word views of g_wldcore_screen_fade_state.flags[1] and priorities[0..1]:
 * the sound-novel setters store whole words where the sort reads a halfword,
 * and the sound-novel menu stores flags[1] by absolute address beside a live
 * &g_wldcore_screen_fade_state base, which the member spelling does not
 * reproduce. */
extern s32 g_wldcore_screen_fade_box_0_priority;
extern s32 g_wldcore_screen_fade_box_1_flags;
extern s32 g_wldcore_screen_fade_box_1_priority;
extern u16* g_wldcore_script_base;
extern void (*g_wldcore_script_opcode_table[])(void);
extern wldcore_point32_t g_wldcore_scroll_text_extra_position;
extern s32 g_wldcore_scroll_text_extra_render_index;
extern u8 g_wldcore_sound_novel_quit_help_text_ids[];
extern struct battle_stats g_wldcore_status_unit;
extern u16* g_wldcore_story_event_ids;
extern s16 g_wldcore_text_bounds[];
/* Provisional: milestone threshold tables at 0x8009eb34 (eight entries,
 * 1/4/8/12/16/20/24/31) and 0x8009eb3c (1/3/6/8/10/12/14/16). */
extern u8 g_wldcore_treasure_count_milestones[];
extern u8 g_wldcore_treasure_picture_sets[];
extern u8 g_wldcore_treasure_sound_novel_ids[];
extern s32 g_wldcore_tutorial_category_saved_cursor;
extern s32 g_wldcore_tutorial_event_ids[];
extern u8 g_wldcore_unexplored_land_count_milestones[];
extern u8 g_wldcore_unexplored_land_picture_sets[];
extern struct world_menu_number_range g_wldcore_window_panel_number_state;
extern wldcore_window_entry_36_rgb_t g_wldcore_window_record_colors[];
/* Twelve {month, day} pairs: the calendar date each zodiac sign starts on,
 * Aries (3/21) first. */
extern u8 g_wldcore_zodiac_start_dates[12][2];
extern s32 g_wldcore_menu_root_window_index;

void wldcore_gfx_draw_world_frame_with_map(void);
void wldcore_init_world_map_state(void);
s32 wldcore_fade_step_screen(void);
void wldcore_gfx_toggle_captured_world_frame(s32 capture);
void wldcore_map_init_tile_atlas(void);
void wldcore_gfx_draw_projected_map_tiles(s32 arg);
void wldcore_window_init_record_pools(void);
void wldcore_gfx_draw_animated_display_object(wldcore_anim_object_t* object, void* ot);
void wldcore_gfx_draw_context_value_display(void*);
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream);
void wldcore_gfx_draw_display_object_sprite(wldcore_display_object_t* object, s32 ot);
void wldcore_list_completed_propositions_clear_result(void);
void wldcore_return_from_job_determinations(s32 job_id);
s32 wldcore_script_process_conditional_set(s32 location, s32 action_mask);
void wldcore_window_build_yes_no_panel(wldcore_point32_t origin, s32 unused_2, s32 unused_3, s32 x2, s32 y2);
void wldcore_gfx_draw_calendar_date(void* ot);
void wldcore_menu_push_location_menu_level(s32 menu);
s32 wldcore_proposition_select_report_speaker(s32 job_id);
void wldcore_proposition_determine_success(void);
void wldcore_proposition_calculate_base_jp_and_gil(void);
void wldcore_proposition_roll_bonus_reward(void);
void wldcore_proposition_apply_extra_days_multiplier(void);
void wldcore_menu_push_event_transition_level(s32 mode);
void wldcore_map_color_and_draw_dots(void);
s32 wldcore_map_build_location_menu_entries(s32 proposition_id, s32* out);
void wldcore_map_draw_visible_routes(void*);
s32 wldcore_location_begin_route_segment(wldcore_location_entry_state_t* state, s32 value, s32 step);
void wldcore_map_draw_path_animation(void*);
void wldcore_map_find_best_route_path(s32 from, s32 to);
void wldcore_map_step_route_travel(void);
s32 wldcore_map_step_projection_motion(void);
void wldcore_sound_process_audio_queue(void);
void wldcore_list_open_script_flags_01a4_01bb(void);
void wldcore_switch_to_stack(void*);
void wldcore_restore_previous_stack(void);
void world_gs_set_display_mode(u16 x, u16 y, u16 intmode, u16 dither, u16 vram);
void world_gs_sortclear(u8 r, u8 g, u8 b, GsOT* otp);
void world_gs_sortsprite(GsSPRITE* sp, GsOT* ot, u16 pri);
void world_gs_swapdispbuff(void);
void world_gs_setworkbase(void* base);
void world_shop_run_screen(s32 shop_type);

/* advance */
void wldcore_advance_calendar_day(void);

/* build */
void wldcore_build_global_file_header(void);

/* clear */
void wldcore_clear_screen_and_set_map_clip_rect(s32 mode);

/* convert */
void wldcore_convert_date_to_zodiac_date(s32* month, s32* day);

/* dispatch */
void wldcore_dispatch_display_object_list(s32 ot, wldcore_display_object_t** list, s32 count);

/* draw */
void wldcore_draw_close_indicator_if_pending(void*);

/* fade */
void wldcore_fade_draw_screen_overlays(void);
void wldcore_fade_start_screen(s32 command, s32 operand);

/* func */
void wldcore_text_render_scrollable_window_page(wldcore_text_scrollable_window_t* state, s32 row);
void wldcore_advance_calendar_days(s32 days);

/* get */
party_data_t* wldcore_get_party_data_pointer(s32 party_index);
s32 wldcore_get_ramza_s_roster_index(void);

/* gfx */
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch);
void wldcore_gfx_copy_color_with_tint(CVECTOR* source, CVECTOR* out);
void wldcore_gfx_draw_world_frame(void);
void wldcore_gfx_init_subsystem(void);
void wldcore_gfx_set_display_rect(s32 buffer_index, s16* rect);

/* handle */
s32 wldcore_handle_exit_request(void);

/* init */
void wldcore_init_input_repeat_state(void);

/* list */
void wldcore_list_build_tutorial_category_panel_image(wldcore_text_list_panel_t* panel);
void wldcore_list_clamp_cursor_state(s32 index, s32 count, s32 visible);
void wldcore_list_clear_cursor_state(s32 index);
void wldcore_list_open_tutorial_topics(void);
void wldcore_list_store_cursor_state(s32 index);

/* load */
void wldcore_load_message_block_if_changed(s32 chapter);
void wldcore_load_data_set_into_scratch_buffer(s32 set);
void wldcore_load_formation_unit_name_and_face(void);
void wldcore_load_scratch_data_blocking(void);
s32 wldcore_proposition_load_send_unit_candidates(wldcore_menu_send_unit_level_t* level);

/* map */
void wldcore_map_apply_clamped_horizontal_scroll(s32* amount, s32* position);
void wldcore_map_apply_clamped_scroll(wldcore_point32_t* amount, wldcore_point32_t* position);
void wldcore_map_apply_clamped_vertical_scroll(wldcore_point32_t* amount, wldcore_point32_t* position);
s32 wldcore_map_find_dot_at_point(wldcore_point32_t point);
void wldcore_map_init_location_marker_and_camera(void);
s32 wldcore_map_is_point_outside_projection_bounds(const wldcore_point32_t* point);
void wldcore_map_negate_and_clamp_coordinates(const s16* input, wldcore_map_clamped_point32_t* output);
void wldcore_map_project_and_cull_dots(void);
void wldcore_map_project_and_cull_tiles(void);
void wldcore_map_pulse_dot_colors(void);
s32 wldcore_map_start_projection_motion_if_outside_bounds(const wldcore_point32_t* point, const s16* coordinates);
void wldcore_map_update_camera_direction_and_projection(void);
void wldcore_map_update_scroll_axis_speed(wldcore_axis_state_t* axis, s32 direction);

/* menu */
/* The list-level steps hand their menu level to these stubs, which read no
 * argument; the parameter is the argument the target passes in $a0. */
void wldcore_list_rumors_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_propositions_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_bar_active_propositions_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_rumors_close_level(wldcore_menu_panel_level_t* level);
void wldcore_list_propositions_close_level(wldcore_menu_panel_level_t* level);
void wldcore_menu_close_level_and_release_records(wldcore_menu_slot_set_t* slots, s32 restore);
void wldcore_menu_dispatch_idle_handler(void);
void wldcore_menu_dispatch_pending_script_requests(void);
void wldcore_menu_dispatch_publish_handler(void);
void wldcore_menu_dispatch_resume_handler(void);
void wldcore_menu_display_text_entry(s32 index, s32 text_id, wldcore_point32_t point, void* buffer);
void wldcore_menu_push_message_level(s32 text, s32 reload_text);
void wldcore_menu_push_number_panel_level_with_argument(s32 variable_id, s32 value);
void wldcore_menu_reset_window_list_and_store_origin(void);
void wldcore_menu_restore_level_on_world_entry(void);
void wldcore_menu_run_world_frame(GsOT* context);
void wldcore_list_unexplored_lands_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_story_events_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_script_variables_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_tutorial_topics_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_formation_tutorials_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_list_script_flags_clear_result(wldcore_menu_panel_level_t* level);
void wldcore_menu_show_next_sequence_message(wldcore_menu_countdown_t* countdown);

/* noop */
void wldcore_noop_8008d514(void);
void wldcore_noop_8008ffd8(void);

/* opcode */
void wldcore_opcode_apply_pending_delta_and_clamp_value(void);
void wldcore_opcode_branch_if_saved_bit(s32 expected);
void wldcore_opcode_branch_if_local_flag(s32 expected);
void wldcore_opcode_branch_if_savedata_bit(s32 expected);
void wldcore_opcode_branch_if_dialog_option(s32 expected);
void wldcore_opcode_load_deep_dungeon_entry_arguments(void);
void wldcore_opcode_load_draw_path_arguments(void);
void wldcore_opcode_load_erase_path_arguments(void);
void wldcore_opcode_load_event(void);
void wldcore_opcode_load_set_var2_arguments(void);
void wldcore_opcode_load_text_id(void);
void wldcore_opcode_start_pending_value_transition(void);

/* proposition */
s32 wldcore_proposition_adjust_message_index(s32 base, s32 party_index);
void wldcore_proposition_advance_daily_counters(void);
s32 wldcore_proposition_count_completed(s32 location);
wldcore_proposition_data_t* wldcore_proposition_get_data_pointer(s32 proposition_index);
s32 wldcore_proposition_load_picture(s32 index);
void wldcore_proposition_load_picture_data_set(s32 value);
s32 wldcore_proposition_step_message(wldcore_proposition_message_state_t* state, s32 message);
void wldcore_proposition_submit_marker_request(s32 ot);

/* reset */
void wldcore_reset_game_if_special_keycode_is_pressed(void);
void wldcore_reset_saved_record_fields(wldcore_saved_record_t* record);
void wldcore_reset_selected_saved_record(void);

/* script */
void wldcore_script_read_operand_pair(s32* out_id, s32* out_value);
void wldcore_script_read_operand_pair_and_date(s32* out_a, s32* out_b, s32* out_c, s32* out_d);

/* set */
void wldcore_set_bit_value(u32* bits, s32 index, s32 set);

/* sound */
void wldcore_sound_change_scenario_music(s32 music_id);
void wldcore_sound_enqueue_audio_command(s32 kind, s32 value);
void wldcore_sound_enqueue_music_start(s32 value);
void wldcore_sound_play_effect(s32 sound_id);
void wldcore_sound_stop_music_and_play_sound(s32 sound_id);
void wldcore_sound_stop_weather_sfx_and_send_quad(s32 sound_id);
void wldcore_sound_wait_for_queue_drain(void);

/* start */
void wldcore_start_core_background_threads(s32 party_index);

s32 wldcore_test_bit(u32* bits, s32 index);

/* text */
void wldcore_text_get_padded_dimensions(s32 text_id, wldcore_text_dimensions_t* output);
void wldcore_text_init_scrollable_window(wldcore_text_scrollable_window_t* state);
s32 wldcore_text_is_window_finished(wldcore_text_scrollable_window_t* window);

/* unpack */
void wldcore_unpack_proposition_row(wldcore_proposition_fields_t* out, s32 index);

/* wait */
void wldcore_wait_and_build_file_header(
    main_file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination);
void wldcore_wait_for_file_load(void);

/* window */
/* Both return the appended record's index. */
s32 wldcore_window_append_record_and_reset_color(u32** queue, s32* count);
s32 wldcore_window_append_render_record_and_reset_color(u32** queue, s32* count);
void wldcore_window_apply_layout(wldcore_window_layout_request_t* request);
void wldcore_window_build_render_record_image(
    s32 index, wldcore_xy16_t position, wldcore_xy16_t dimensions, s32 coordinate_mode, u32* image);
void wldcore_window_draw_centered_text(s32 window_index, s32 text_id);
void wldcore_window_draw_indexed_text(wldcore_window_text_table_t* table, s32 index);
void wldcore_window_draw_text_rows(wldcore_menu_send_unit_level_t* level);
void wldcore_window_init_number_panel_render_thread(
    s32 thread_id, u32 packed_values, s32 text_id, wldcore_point32_t* origin);
void wldcore_window_init_panel_render_state(
    s32 unused, s32 extent, wldcore_point32_t* dimensions, wldcore_point32_t* origin);
void wldcore_window_init_record(s32 index, s32 priority, s32 sequence);
void wldcore_window_init_render_record_image(
    s32 index, wldcore_xy16_t position, wldcore_xy16_t dimensions, s32 coordinate_mode, s32 text_id, u32* image);
void wldcore_window_init_vram_render_record(
    s32 index, wldcore_xy16_t position, wldcore_xy16_t dimensions, wldcore_point32_t clut, s32 flags);
void wldcore_window_load_image_record_to_vram(s32 index, u32* image);
void wldcore_window_reset_state_and_scroll(wldcore_menu_stack_record_t* level);
void wldcore_window_set_render_objects_visible(s32 clear);
void wldcore_window_set_render_state_2_from_arg_0x04(s32* param);
void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_point32_t* dimensions, wldcore_point32_t* origin);
void wldcore_window_transform_point_for_mode(s32 mode, wldcore_window_render_bounds16_t bounds, s32* output);

/* bin */
s32 wldcore_entrypoint(void);

/* finalize */
void wldcore_finalize_loop(void);

/* initialize */
void wldcore_init_core(void);
void wldcore_init_runtime_state(void);
void wldcore_init_subsystems(void);

/* process */
void wldcore_process_frame_gate(void);
/* WLDCORE script interpreter state; the opcode handlers read ip/data here.
 * One object through the choice/argument words:
 * wldcore_opcode_read_four_halfword_pairs reaches ip at -4 and the choice
 * values at +0x10 from its &choice_vars base. */
typedef struct wldcore_script_state {
    s32 flags;          /* 0x00 */
    u16* list;          /* 0x04; script list for the current location */
    u16* data;          /* 0x08 */
    u16 ip;             /* 0x0c */
    s32 choice_vars[4]; /* 0x10; staged Choice variables */
    /* 0x20: script request arguments; the staged Choice values share them. */
    s32 args[4];
} wldcore_script_state_t;

typedef char wldcore_script_state_size_must_be_0x30[sizeof(wldcore_script_state_t) == 0x30 ? 1 : -1];

extern wldcore_script_state_t g_wldcore_script_state;
/* Scalar view of g_wldcore_script_state.flags for the opcode handlers that
 * read-modify-write it: the member spelling there shares one base register
 * with the args/data accesses instead of the target's absolute addresses.
 * The load_* opcode handlers declare volatile views of flags/data/ip locally. */
extern s32 g_wldcore_script_flags;

#endif
