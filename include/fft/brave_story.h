#ifndef BRAVE_STORY_H
#define BRAVE_STORY_H

/* The Brave Story record: facts, events, persons, the Propositions sections (Advancing, Work history), unexplored lands
 * and treasures. */

#include "fft/menu_selection.h"
#include "fft/menu_stack.h"
#include "psx/types.h"

/* story list */
/* Row kinds of the Brave Story list (story_rows_build), named after the level each row
 * opens. */
typedef enum brave_story_row {
    BRAVE_STORY_ROW_FACTS = 0, /* EVENT_SCRIPT_VAR_FACT_FIRST */
    BRAVE_STORY_ROW_EVENTS = 1,
    BRAVE_STORY_ROW_PERSONS = 2, /* EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST */
    BRAVE_STORY_ROW_PROPOSITIONS = 3,
    BRAVE_STORY_ROW_UNEXPLORED_LANDS = 4,
    BRAVE_STORY_ROW_TREASURES = 5,
    BRAVE_STORY_ROW_LEAVE = 6,
    BRAVE_STORY_ROW_COUNT = 7
} brave_story_row_e;

/* The root Brave Story menu, beneath its category and detail panels. */
typedef struct brave_story_menu_level {
    s32 cursor_window;
    s32 label_window;   /* 0x04 */
    s32 rows_image;     /* 0x08 */
    s32 casualty_image; /* 0x0c */
    u8 _unused_10[8];
    s32 mode;                     /* 0x18; menu_stack_window_mode_e */
    s32 selected_entry;           /* 0x1c; initial row from g_brave_story_saved_cursor */
    s32 land_completion_star;     /* 0x20 */
    s32 treasure_completion_star; /* 0x24 */
    u8 _unused_28[8];
    s32 entry_count;                    /* 0x30; rows drawn by brave_story_menu_open */
    s32 entries[BRAVE_STORY_ROW_COUNT]; /* 0x34; brave_story_row_e; text is TEXT_ID_BRAVE_STORY_ROW_FIRST + kind */
} brave_story_menu_level_t;
typedef char brave_story_menu_level_size_must_be_80[sizeof(brave_story_menu_level_t) == 80 ? 1 : -1];

/* Selected-row alias of MENU_STACK_CURSOR_SLOT_BRAVE_STORY; retained for load ordering. */
extern s32 g_brave_story_saved_cursor[];

void brave_story_menu_open(void);
void brave_story_menu_update(menu_stack_record_t* level);

/* detail levels */
/* brave_story_detail_level_t.phase, shared by the person,
 * treasure and unexplored-land detail steps. */
typedef enum brave_story_detail_phase {
    BRAVE_STORY_DETAIL_PHASE_SHOWN = 0,
    BRAVE_STORY_DETAIL_PHASE_LOAD = 1,  /* waits for the file */
    BRAVE_STORY_DETAIL_PHASE_BUILD = 2, /* waits for the message box, then builds the text and picture */
    BRAVE_STORY_DETAIL_PHASE_POP_IN = 3 /* waits for the text window's opening animation */
} brave_story_detail_phase_e;

/* Provisional: the BRAVE_STORY_PERSON level pushed by person_detail_open over the list-window
 * level; it keeps the selected entry value (person * 16 + profile) and the render record it appends. The
 * treasure and unexplored-land details use the same record. */
typedef struct brave_story_detail_level {
    s32 value;        /* 0x00 */
    s32 render_index; /* 0x04 */
    /* 0x08; no push sets it. The treasure detail stores the saved record of the treasure's sound novel
     * here before it leaves. The treasure and unexplored-land details also read it in their LOAD phase,
     * as a render record index, when it still holds what the last level at this depth left. */
    s32 saved_record_index;
    s32 phase; /* 0x0c; brave_story_detail_phase_e; LOAD at push */
    s32 timer; /* 0x10; cleared at push; text delay or dissolve step, counted per frame */
} brave_story_detail_level_t;

/* facts and events */
/* A story event replayed from the Brave Story list: the event, the list cursor
 * to restore and the weather variable to put back afterwards. */
extern s32 g_brave_story_replay_story_event_index;
extern menu_cursor_state_t g_brave_story_saved_list_cursor_state;
extern s32 g_brave_story_saved_weather_variable;

void brave_story_fact_list_open(void);
void brave_story_block_fact_list_buttons(menu_stack_record_t* unused_level);
void brave_story_fact_list_input_update(menu_stack_record_t* level);
void brave_story_fact_list_buttons_restore(menu_stack_record_t* level);
void brave_story_wait_fact_then_pop(menu_stack_record_t* level);
void brave_story_event_level_open(s32 value);
void brave_story_event_input_update(menu_stack_record_t* level);
void brave_story_block_event_list_buttons(menu_stack_record_t* unused_level);
void brave_story_event_list_input_update(menu_stack_record_t* level);
void brave_story_event_list_prompt_restore(menu_stack_record_t* level);

/* persons */
extern struct calendar_date* g_brave_story_birthdays;
extern u8 g_brave_story_character_ages[0x40];

void brave_story_block_person_list_buttons(menu_stack_record_t* unused_level);
void brave_story_person_list_input_update(menu_stack_record_t* level);
void brave_story_person_list_prompt_restore(menu_stack_record_t* level);
void brave_story_person_level_update(menu_stack_record_t* level);

/* unexplored lands and treasures */
extern u8 g_unexplored_land_picture_sets[];
extern u8 g_treasure_picture_sets[];

/* RAM bitmaps indexed by 32-bit values; the save serializer copies only their packed data bytes and omits the
 * padding at the end of each allocation. */
extern u32 g_brave_story_land_discovery_date_bits[5];
extern u32 g_brave_story_treasure_acquisition_date_bits[14];

void brave_story_unexplored_land_list_open(void);
void brave_story_block_unexplored_land_list_buttons(menu_stack_record_t* unused_level);
void brave_story_unexplored_land_list_input_update(menu_stack_record_t* level);
void brave_story_unexplored_land_list_prompt_restore(menu_stack_record_t* level);
void brave_story_unexplored_land_level_update(menu_stack_record_t* level);
void brave_story_treasure_list_open(void);
void brave_story_treasure_level_open(s32 value);
void brave_story_treasure_list_input_update(menu_stack_record_t* level);
void brave_story_block_treasure_list_buttons(menu_stack_record_t* level);
void brave_story_treasure_list_prompt_restore(menu_stack_record_t* level);
void brave_story_treasure_level_update(menu_stack_record_t* level);

/* propositions */
/* Row kinds of the Brave Story's proposition category list (brave_story_propositions_level_open). */
typedef enum brave_story_proposition_row {
    BRAVE_STORY_PROPOSITION_ROW_ADVANCING = 0,
    BRAVE_STORY_PROPOSITION_ROW_WORK_HISTORY = 1,
    BRAVE_STORY_PROPOSITION_ROW_COUNT = 2
} brave_story_proposition_row_e;

/* The Propositions category level owns a cursor and a row image, unlike the
 * Advancing level's cursor and three column labels. */
typedef struct brave_story_propositions_level {
    s32 cursor_window;
    s32 rows_image; /* 0x04 */
    u8 _unused_08[4];
    /* 0x0c; Work History hides and restores this image, but the category push
     * never initializes it. Preserve the previous contents of the stack slot. */
    s32 retained_image_index;
    u8 _unused_10[0x0c];
    s32 selected_entry; /* 0x1c */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[BRAVE_STORY_PROPOSITION_ROW_COUNT];
} brave_story_propositions_level_t;
typedef char brave_story_propositions_level_size_must_be_60[sizeof(brave_story_propositions_level_t) == 60 ? 1 : -1];

/* Provisional: the BRAVE_STORY_ADVANCING level pushed by advancing_list_open: a cursor window and
 * three column-header windows over one render record listing each active proposition's name, participant count and
 * remaining days. */
typedef struct brave_story_advancing_level {
    s32 cursor_window; /* 0x00; DISPLAY_OBJECT_ANIM_CURSOR_IDLE, moved to the cursor row */
    s32 title_window;  /* 0x04; DISPLAY_OBJECT_ANIM_PROPOSITION_TITLE_LABEL */
    s32 count_window;  /* 0x08; DISPLAY_OBJECT_ANIM_PROPOSITION_COUNT_LABEL */
    s32 days_window;   /* 0x0c; DISPLAY_OBJECT_ANIM_PROPOSITION_DAYS_LABEL */
    s32 render_index;  /* 0x10 */
    u8 _unused_14[8];
    s32 cursor; /* 0x1c; initial row from g_brave_story_advancing_saved_cursor */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34; indices into g_proposition_active */
} brave_story_advancing_level_t;

/* The BRAVE_STORY_PARTICIPANTS level. The detail-panel builder fills the render index and row count before the cursor
 * is positioned. */
typedef struct brave_story_participant_list_level {
    s32 cursor_window; /* 0x00 */
    u8 _unused_04[4];
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
    s32 proposition;  /* 0x10 */
    u8 _unused_14[4];
    s32 mode;   /* 0x18; set to MENU_STACK_WINDOW_MODE_BROWSE */
    s32 cursor; /* 0x1c */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
} brave_story_participant_list_level_t;

/* Provisional: the BRAVE_STORY_PARTICIPANT level pushed by participant_detail_open with two frame
 * windows; unit_panel_party_unit_prepare draws the participant
 * g_proposition_active[proposition].participant_indices[participant]. */
typedef struct brave_story_participant_level {
    s32 left_window;  /* 0x00 */
    s32 right_window; /* 0x04 */
    s32 _unknown_08;  /* 0x08; cleared at push */
    s32 _unused_0c;   /* 0x0c */
    s32 proposition;  /* 0x10; index into g_proposition_active */
    s32 participant;  /* 0x14; slot in participant_indices */
} brave_story_participant_level_t;

/* Selected-row alias of MENU_STACK_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS; the array declaration
 * preserves the target's load order. */
extern s32 g_brave_story_propositions_saved_cursor[];

void brave_story_participant_list_input_update(menu_stack_record_t* level);
void brave_story_propositions_input_update(menu_stack_record_t* level);
void brave_story_advancing_list_input_update(menu_stack_record_t* level);
void brave_story_participant_input_update(menu_stack_record_t* level);
void brave_story_work_history_input_update(menu_stack_record_t* level);
void brave_story_wait_work_history_then_pop(menu_stack_record_t* level);

/* level window state */
/* Dim, tint or restore the windows owned by the current menu level. */
void brave_story_menu_suspend(menu_stack_record_t* level);
void brave_story_menu_resume(menu_stack_record_t* level);
void brave_story_propositions_level_open(void);
void brave_story_propositions_level_resume(menu_stack_record_t* level);
void brave_story_propositions_level_suspend(menu_stack_record_t* level);
void brave_story_advancing_list_resume(menu_stack_record_t* level);
void brave_story_advancing_list_suspend(menu_stack_record_t* level);
void brave_story_participant_level_resume(menu_stack_record_t* level);
void brave_story_participant_level_suspend(menu_stack_record_t* level);
void brave_story_work_history_suspend(menu_stack_record_t* level);
void brave_story_work_history_resume(menu_stack_record_t* level);

#endif
