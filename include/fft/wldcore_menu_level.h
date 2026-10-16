#ifndef WLDCORE_MENU_LEVEL_H
#define WLDCORE_MENU_LEVEL_H

/* WLDCORE's menu levels: the level types, the record each push routine fills in its 0x5c-byte slot of the menu stack
 * (grouped by the screens that push them), and the union the stack keeps them in. */

#include "psx/types.h"

/* level types */
/* The type of a menu level: it selects the level's idle, suspend and resume handlers. Push routines fill the level
 * record; RETURN bridges the entry window and a pending world-map transition. */
typedef enum menu_stack_level {
    MENU_STACK_LEVEL_WORLD_MAP = 0,
    MENU_STACK_LEVEL_WORLD_MENU = 1,
    MENU_STACK_LEVEL_TEXT_WINDOW = 2,
    MENU_STACK_LEVEL_MESSAGE = 3,
    MENU_STACK_LEVEL_LOCATION = 4,
    MENU_STACK_LEVEL_BAR = 5,
    MENU_STACK_LEVEL_RUMOR_LIST = 6,
    MENU_STACK_LEVEL_PROPOSITION_AVAILABLE_LIST = 7,
    MENU_STACK_LEVEL_RUMOR_DETAIL = 8,
    MENU_STACK_LEVEL_PROPOSITION_DETAIL = 9,
    MENU_STACK_LEVEL_PROPOSITION_ACCEPT = 10,
    MENU_STACK_LEVEL_DEBUG_VARIABLE_LIST = 11,
    MENU_STACK_LEVEL_PROPOSITION_DAYS = 12,
    MENU_STACK_LEVEL_PROPOSITION_DISPATCH_CONFIRM = 13,
    MENU_STACK_LEVEL_PROPOSITION_RETURNED_LIST = 14,
    MENU_STACK_LEVEL_PROPOSITION_REPORT_CONFIRM = 15,
    MENU_STACK_LEVEL_PROPOSITION_REPORT = 16,
    MENU_STACK_LEVEL_PROPOSITION_RESULT = 17,
    MENU_STACK_LEVEL_PROPOSITION_ACTIVE_LIST = 18,
    MENU_STACK_LEVEL_PROPOSITION_RECALL = 19,
    MENU_STACK_LEVEL_SOUND_NOVEL_RESTART = 20,
    MENU_STACK_LEVEL_SOUND_NOVEL = 21,
    MENU_STACK_LEVEL_SOUND_NOVEL_QUIT = 22,
    MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY = 23,
    MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LANDS = 24,
    MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND = 25,
    MENU_STACK_LEVEL_BRAVE_STORY_TREASURES = 26,
    MENU_STACK_LEVEL_BRAVE_STORY_TREASURE = 27,
    MENU_STACK_LEVEL_BRAVE_STORY = 28,
    MENU_STACK_LEVEL_BRAVE_STORY_EVENTS = 29,
    MENU_STACK_LEVEL_BRAVE_STORY_EVENT = 30,
    MENU_STACK_LEVEL_BRAVE_STORY_PERSONS = 31,
    MENU_STACK_LEVEL_BRAVE_STORY_PERSON = 32,
    MENU_STACK_LEVEL_TUTORIAL_TOPICS = 33,
    MENU_STACK_LEVEL_TUTORIAL_TOPIC = 34,
    MENU_STACK_LEVEL_EVENT_TRANSITION = 35,
    MENU_STACK_LEVEL_PROPOSITION_SEND_UNIT = 36,
    MENU_STACK_LEVEL_BRAVE_STORY_PROPOSITIONS = 37,
    MENU_STACK_LEVEL_BRAVE_STORY_ADVANCING = 38,
    MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANTS = 39,
    MENU_STACK_LEVEL_BRAVE_STORY_PARTICIPANT = 40,
    MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY = 41,
    MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY = 42,
    MENU_STACK_LEVEL_TUTORIAL = 43,
    MENU_STACK_LEVEL_TUTORIAL_FORMATION_LIST = 44,
    MENU_STACK_LEVEL_TUTORIAL_BATTLE_LIST = 45,
    MENU_STACK_LEVEL_BRAVE_STORY_FACTS = 46,
    MENU_STACK_LEVEL_BRAVE_STORY_FACT = 47,
    MENU_STACK_LEVEL_DEBUG_VARIABLE_EDIT = 48,
    MENU_STACK_LEVEL_SCREEN_TRANSITION = 49,
    MENU_STACK_LEVEL_WORLD_MAP_FOCUS = 50,
    MENU_STACK_LEVEL_SCROLLABLE_TEXT = 51,
    MENU_STACK_LEVEL_DEBUG_PARTY_NUMBER = 52,
    MENU_STACK_LEVEL_WORLD_MAP_PATH = 53,
    MENU_STACK_LEVEL_WORLD_MAP_LOCATION_VISIBILITY = 54,
    MENU_STACK_LEVEL_PROPOSITION_DISCOVERY = 55,
    MENU_STACK_LEVEL_UNIT_PANEL_HELP = 56,
} menu_stack_level_e;

/* shared level parts */
/* A window record and a render record handled together (wldcore_window_set_render_palette_2). */
typedef struct window_render_pair {
    s32 window_index;
    s32 render_index;
} window_render_pair_t;

/* Leading 32-bit fields of a menu level that owns up to two window/render record
 * pairs; menu_stack_pop_and_release_records releases each index
 * that is not -1. */
typedef struct menu_stack_slot_set {
    s32 result;   /* 0x00; the location menu step sets 1 on confirm, 0 on cancel */
    s32 window_a; /* 0x04 */
    s32 render_a; /* 0x08 */
    s32 window_b; /* 0x0c */
    s32 render_b; /* 0x10 */
} menu_stack_slot_set_t;

/* The window-layout request that opens a LAYOUT_WINDOW level. unit_panel_move_help_cursor reads the first three
 * fields. */
typedef struct unit_panel_help_level {
    s32 window_index;
    s32 layout; /* 0x04; row of g_unit_panel_help_rows */
    s32 cached_layout_kind;
    s32 saved_state; /* 0x0c; previous g_menu_stack_ot_offset, saved by the push */
} unit_panel_help_level_t;

/* entry, messages and text windows */
/* Provisional: the RETURN level that closes the world map, pushed by world_map_push_menu_level.
 * world_map_step_menu_level is its only reader: delay counts down the frames the fade needs and pending latches
 * while SYSTEM_FLAG_SCREEN_CLEARED is up. */
typedef struct world_map_menu_level {
    s32 delay;
    s32 pending; /* 0x04 */
} world_map_menu_level_t;

/* The MESSAGE and TEXT_WINDOW levels (menu_stack_push_message_level, menu_stack_push_text_window_level). */
typedef struct menu_stack_message_level {
    s32 reload_text;
    s32 saved_blocked_buttons; /* 0x04; g_menu_blocked_buttons at push time */
    s32 window_index;          /* 0x08; window record appended by the TEXT_WINDOW push; cleared by the MESSAGE push */
    s32 _unknown_0c;           /* 0x0c; set to 8 at push */
} menu_stack_message_level_t;

/* The SCROLLABLE_TEXT_WINDOW level, pushed by menu_stack_push_scrollable_text_level and stepped by
 * menu_stack_step_scrollable_text_level. */
typedef struct menu_stack_scrollable_text_level {
    s32 window_index; /* 0x00; window record of the frame */
    u8 _unused_04[0x1c];
    s32 _unknown_20; /* 0x20; 0x28 at push; nothing reads it */
} menu_stack_scrollable_text_level_t;

/* A level owning two window records; sound_novel_opcode_0c_set_parent_windows_visible toggles both records'
 * DISPLAY_OBJECT_HIDDEN. */
typedef struct window_pair_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
} window_pair_level_t;

/* A level owning two window records and a render record: bar_push_level pushes it as BAR_MENU.
 * tutorial_push_topic_level hides all three (DISPLAY_OBJECT_HIDDEN) and
 * tutorial_wait_topic_then_pop shows them again. */
typedef struct window_pair_render_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 panel_rows;    /* 0x0c; cleared by bar_push_level */
} window_pair_render_level_t;

/* Provisional: owner of a text-list render record, as
 * tutorial_build_menu_panel reads it; only the record
 * index, row count and text-id rows are established. */
typedef struct tutorial_menu_panel {
    u8 _unused_00[8];
    s32 render_index; /* 0x08 */
    u8 _unused_0c[0x24];
    s32 row_count;   /* 0x30 */
    s32 text_ids[1]; /* 0x34; row_count entries, relative to TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST */
} tutorial_menu_panel_t;

/* lists, panels and details */
/* Panel levels: the number panels (proposition_push_days_level, debug_push_variable_edit_level and
 * debug_push_party_number_level) and the list
 * panels (brave_story_open_unexplored_land_list, tutorial_open_topic_list and
 * bar_leave). */
typedef struct menu_stack_panel_level {
    /* 0x00; number panel: 0 running, 1 confirmed, 2 canceled; list panels
     * also use -1 for cancellation. Read after the panel thread (WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) ends. */
    s32 result;
    s32 variable_id; /* 0x04; script variable edited by number panels */
    u8 _unused_08[0x28];
    s32 entry_count; /* 0x30; rows in the panel list */
    /* 0x34; entry text ids, filled by location_build_menu_entries, whose result is the entry count;
     * the same offset as the list_window and proposition_list entries */
    s32 entries[10];
} menu_stack_panel_level_t;

/* Phases of a list-window level (menu_stack_window_level_t.mode). The
 * town menu uses only the first two. */
typedef enum menu_stack_window_mode {
    MENU_STACK_WINDOW_MODE_BROWSE = 0,
    MENU_STACK_WINDOW_MODE_LEAVE_MESSAGE = 1, /* the farewell text is up; waits for circle or cross */
    MENU_STACK_WINDOW_MODE_FADE_OR_HELP = 2,  /* fading out after the farewell, or under the help message */
    MENU_STACK_WINDOW_MODE_LEAVE_WAIT = 3,    /* faded out; waits for thread 14 to end, then pops */
} menu_stack_window_mode_e;

/* Provisional: the list-window level of the bar, the Brave Story and the tutorials, beneath their flag-list panels.
 * brave_story_open_work_history and brave_story_open_person_list hide its content render record when they
 * push; brave_story_wait_fact_then_pop shows its upper and lower windows again when it pops back to it. */
typedef struct menu_stack_window_level {
    s32 main_window;
    s32 side_window;    /* 0x04 */
    s32 frame_render;   /* 0x08 */
    s32 content_render; /* 0x0c */
    u8 _unused_10[8];
    s32 mode;           /* 0x18; menu_stack_window_mode_e */
    s32 selected_entry; /* 0x1c; initial row from g_brave_story_saved_cursor */
    s32 upper_window;   /* 0x20; Brave Story: completion star of the unexplored lands row */
    s32 lower_window;   /* 0x24; Brave Story: completion star of the treasures row */
    u8 _unused_28[8];
    s32 entry_count; /* 0x30; rows drawn by brave_story_push_level */
    s32 entries[10]; /* 0x34; row kinds of the owning list (wldcore_*_row_e); Brave Story text is
                      * TEXT_ID_BRAVE_STORY_ROW_FIRST + kind */
} menu_stack_window_level_t;

/* Provisional: the debug VARIABLE_LIST level pushed by debug_push_variable_list_level;
 * debug_draw_variable_list draws row_count variables from first_variable into the render record. */
typedef struct debug_variable_list_level {
    s32 window_index;   /* 0x00; cursor window */
    s32 render_index;   /* 0x04 */
    s32 first_variable; /* 0x08 */
    s32 cursor;         /* 0x0c; cursor row */
    u8 _unused_10[0x20];
    s32 row_count; /* 0x30 */
} debug_variable_list_level_t;

/* brave_story_detail_level_t.phase, shared by the person,
 * treasure and unexplored-land detail steps. */
typedef enum brave_story_detail_phase {
    BRAVE_STORY_DETAIL_PHASE_SHOWN = 0,
    BRAVE_STORY_DETAIL_PHASE_LOAD = 1,   /* waits for the file */
    BRAVE_STORY_DETAIL_PHASE_BUILD = 2,  /* waits for the message box, then builds the text and picture */
    BRAVE_STORY_DETAIL_PHASE_POP_IN = 3, /* waits for the text window's opening animation */
} brave_story_detail_phase_e;

/* Provisional: the PERSON_DETAIL level pushed by brave_story_push_person_level over the list-window
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

/* transitions and announcements */
/* Provisional: the SCREEN_TRANSITION level pushed by screen_transition_push_level and stepped by
 * screen_transition_step_level. */
typedef struct screen_transition_level {
    s32 screen;     /* 0x00; menu_stack_level_e of the level to open */
    s32 param_a;    /* 0x04 */
    s32 param_b;    /* 0x08 */
    s32 _unused_0c; /* 0x0c */
    s32 delay;      /* 0x10; 0x14 at push */
    s32 phase;      /* 0x14; cleared at push */
} screen_transition_level_t;

/* event_transition_level_t.mode, the argument of
 * event_transition_push_level and the second operand of the
 * world script's load-event instruction. Any value but 0 and 2 acts as 1. */
typedef enum event_transition_mode {
    EVENT_TRANSITION_MODE_SPIN = 0, /* spins the screen in and back out: a random battle */
    /* Spins the screen in with a second sound and whites it out: every
     * battle the world scripts start. */
    EVENT_TRANSITION_MODE_SPIN_IN = 1,
    EVENT_TRANSITION_MODE_FADE = 2, /* a plain screen fade: every scene they start */
} event_transition_mode_e;

/* event_transition_level_t.phase. */
typedef enum event_transition_phase {
    EVENT_TRANSITION_PHASE_SPIN_IN = 0,
    EVENT_TRANSITION_PHASE_SPIN_OUT = 1,
    EVENT_TRANSITION_PHASE_DONE = 2,
} event_transition_phase_e;

/* event_transition_level_t.fade_phase. */
typedef enum event_transition_fade {
    EVENT_TRANSITION_FADE_NONE = 0,
    EVENT_TRANSITION_FADE_FLASH = 1,     /* the fill box ramps by 0x10 up to 0x80, then restarts as WHITE_OUT */
    EVENT_TRANSITION_FADE_WHITE_OUT = 2, /* ramps by 8 to white, then leaves the world loop */
} event_transition_fade_e;

/* event_transition_level_t.drift_direction. */
typedef enum event_transition_drift {
    EVENT_TRANSITION_DRIFT_RIGHT = 0x01, /* clear: left */
    EVENT_TRANSITION_DRIFT_DOWN = 0x02,  /* clear: up */
} event_transition_drift_e;

/* Provisional: the EVENT_TRANSITION level pushed by event_transition_push_level and stepped by
 * event_transition_step_level, which spins or fades the screen out before an event.
 * event_transition_draw_sprite draws g_event_transition_sprite while the parent level's mode
 * is not 2. */
typedef struct event_transition_level {
    s32 frame;           /* 0x00; counts step frames; the sprite updates every third, 1 restores the captured screen */
    s32 drift_direction; /* 0x04; event_transition_drift_e; rand() * 4 >> 15 at push */
    s32 spin_direction;  /* 0x08; rand() * 2 >> 15 at push; selects +/-0x2000 rotation per update */
    s32 phase;           /* 0x0c; event_transition_phase_e */
    s32 fade_phase;      /* 0x10; event_transition_fade_e */
    s32 anim_step;       /* 0x14; +2/+4 per update; drives scale, shade, sounds 0x6c/0x6d and phase changes */
    u8 _unused_18[8];
    s32 mode; /* 0x20; event_transition_mode_e; argument of the push */
} event_transition_level_t;

/* proposition_discovery_level_t.phase. */
typedef enum proposition_discovery_phase {
    PROPOSITION_DISCOVERY_PHASE_MESSAGES = 0,     /* circle shows the next message */
    PROPOSITION_DISCOVERY_PHASE_LAST_MESSAGE = 1, /* waits for the message box, then starts MUSIC_MILESTONE */
    PROPOSITION_DISCOVERY_PHASE_JINGLE = 2,       /* 180 frames, then back to MUSIC_BAR */
    PROPOSITION_DISCOVERY_PHASE_CLOSE = 3,        /* waits for the audio queue, then pops */
} proposition_discovery_phase_e;

/* The DISCOVERY level pushed by proposition_push_discovery_level after a report that found treasure or
 * unexplored land: a run of "found" messages ending in the milestone jingle. proposition_show_next_milestone_message
 * shows each message and sets LAST_MESSAGE when the count runs out; proposition_step_discovery_level then advances
 * the later phases. */
typedef struct proposition_discovery_level {
    s32 current;
    s32 remaining;
    s32 phase; /* 0x08; proposition_discovery_phase_e */
    s32 timer; /* 0x0c; frames spent in phase 2 */
} proposition_discovery_level_t;

/* map and locations */
/* Provisional: the world-map cursor level (ROOT) stepped by world_map_step_cursor_level. */
typedef struct world_map_cursor_level {
    s32 window_index;     /* 0x00; the map cursor window */
    s32 alt_window_index; /* 0x04; the wide-view window swapped in by 0x80 */
    s32 wide_view;        /* 0x08; non-zero while the wide view is shown */
    s32 snap_hit;         /* 0x0c; dot index returned by the snap probe */
    s32 pending_mode;     /* 0x10 */
} world_map_cursor_level_t;

/* The MAP_LOCATION_VISIBILITY level pushed by world_map_push_location_visibility_level and stepped by
 * world_map_step_location_visibility_level; only the first four fields are known. */
typedef struct world_map_visibility_level {
    s32 dot_index;     /* 0x00; map dot, and script variable 0x200 + dot_index */
    s32 was_set;       /* 0x04; whether the variable was already set */
    s32 delay;         /* 0x08; frames the step handler waits, 0x10 here */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} world_map_visibility_level_t;
typedef char wldcore_map_visibility_level_size_must_be_92[sizeof(world_map_visibility_level_t) == 92 ? 1 : -1];

/* Provisional: the MAP_PATH level pushed by world_map_push_path_level, decoded from the four fields it
 * writes. Its step handler world_map_step_path_level reads only the last two. The first two are the script's
 * path arguments, which location_script_dispatch_requests passes from g_location_script_state.args. Only
 * the first four fields of the slot are known. */
typedef struct world_map_path_level {
    s32 path_id;       /* 0x00; g_location_script_state.args[0] */
    s32 path_id_hi;    /* 0x04; g_location_script_state.args[1] */
    s32 erase;         /* 0x08; step handler walks the path backwards when set */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} world_map_path_level_t;
typedef char wldcore_map_path_level_size_must_be_92[sizeof(world_map_path_level_t) == 92 ? 1 : -1];

/* Provisional: the location menu level as its step handler
 * location_step_menu_level sees it. */
typedef struct location_menu_state {
    menu_stack_slot_set_t slots; /* 0x00; slots.result is the confirmed choice */
    s32 delay;                   /* 0x14 */
    s32 fade;                    /* 0x18 */
    u8 _unused_1c[0x14];
    s32 entry_count; /* 0x30 */
} location_menu_state_t;

/* Provisional: the LOCATION_MENU level as location_push_menu_level fills it (the step handler's view
 * is location_menu_state_t). */
typedef struct location_menu_level {
    s32 window_index;  /* 0x00 */
    s32 cursor_window; /* 0x04; -1 until the cursor record is appended */
    s32 frame_render;  /* 0x08; -1 when there is no picture */
    s32 label_window;  /* 0x0c; -1 when there is no picture */
    s32 extra_render;  /* 0x10; -1 at push; released on close when set (menu_stack_slot_set_t.render_b) */
    s32 delay;         /* 0x14; 8 at push, counted down by the location step (location_menu_state_t.delay) */
    s32 mode;          /* 0x18; 0 then 8 */
    s32 selected_entry;
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34 */
} location_menu_level_t;

/* Provisional: the level a route travel runs under. travel_step uses the leading window index,
 * the location id, the pending result, and the choice text ids it collects when the travel ends. */
typedef struct travel_level {
    s32 window_index; /* 0x00 */
    u8 _unused_04[8];
    s32 location_id; /* 0x0c */
    u8 _unused_10[0x20];
    s32 pending_result;      /* 0x30 */
    s32 choice_text_ids[10]; /* 0x34; retained as location_list.entries after arrival */
} travel_level_t;
typedef char wldcore_menu_location_transition_level_size_must_be_92[sizeof(travel_level_t) == 92 ? 1 : -1];

/* bar and propositions */
/* The RUMOR_DETAIL level pushed by rumor_push_detail_level. Its close handler
 * rumor_wait_detail_then_pop reads the saved parent window and render indices. */
typedef struct rumor_detail_level {
    s32 rumor_index;
    s32 _unused_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} rumor_detail_level_t;

/* proposition_confirm_level_t.mode as proposition_handle_accept_confirm_input steps it. */
typedef enum proposition_accept_mode {
    PROPOSITION_ACCEPT_MODE_CHOOSE = 0,
    PROPOSITION_ACCEPT_MODE_NO_UNIT_MESSAGE = 1, /* nobody can go: a press shows the second message */
    PROPOSITION_ACCEPT_MODE_CLOSE_MESSAGE = 3,   /* a refusal message is up: a press closes the panel */
} proposition_accept_mode_e;

/* The yes/no panel built by window_build_yes_no_panel and stepped by
 * proposition_handle_accept_confirm_input and proposition_handle_dispatch_confirm_input. */
typedef struct proposition_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 render_index;  /* 0x04 */
    s32 row;           /* 0x08; 0 = yes, 1 = no */
    s32 mode;          /* 0x0c; proposition_accept_mode_e */
    s32 header_window; /* 0x10 */
} proposition_confirm_level_t;

/* proposition_send_unit_level_t.mode. */
typedef enum proposition_send_unit_mode {
    PROPOSITION_SEND_UNIT_MODE_SELECT = 0,
    PROPOSITION_SEND_UNIT_MODE_CANCEL = 1,       /* cross: closes back to the available propositions */
    PROPOSITION_SEND_UNIT_MODE_REPLACE = 2,      /* three are chosen: picks the slot the unit replaces */
    PROPOSITION_SEND_UNIT_MODE_STATUS_OPEN = 3,  /* square: waits for the list threads, then opens the status panels */
    PROPOSITION_SEND_UNIT_MODE_STATUS = 4,       /* unit status, browsed with L1 and R1 */
    PROPOSITION_SEND_UNIT_MODE_STATUS_CLOSE = 5, /* waits for the status threads, then reopens the list */
    PROPOSITION_SEND_UNIT_MODE_ACCEPT = 6,       /* start: closes and moves on to the day count */
} proposition_send_unit_mode_e;

/* Provisional: the PROPOSITION_SEND_UNIT level, where up to three units are chosen for a proposition.
 * proposition_step_send_unit_slots selects the slot and owns the portrait render records. */
typedef struct proposition_send_unit_level {
    window_render_pair_t list_frame; /* 0x00 */
    s32 frame_window_a;              /* 0x08 */
    s32 frame_window_b;              /* 0x0c */
    s32 preview_render;              /* 0x10 */
    s32 list_render;                 /* 0x14 */
    s32 cursor_window;               /* 0x18 */
    u8 _unused_1c[4];
    s32 mode;          /* 0x20; proposition_send_unit_mode_e */
    s32 shown_preview; /* 0x24 */
    s32 slot;          /* 0x28 */
    u8 _unused_2c[4];
    s32 unit_count;      /* 0x30; wrap bound, written by proposition_load_send_unit_candidates */
    s32 portrait_render; /* 0x34 */
    s32 portrait_frame;  /* 0x38 */
    s32 portrait_window; /* 0x3c */
    s32 left_arrow;      /* 0x40 */
    s32 right_arrow;     /* 0x44 */
    u8 _unused_48[0x10];
    s32 shown_unit; /* 0x58 */
} proposition_send_unit_level_t;
typedef char wldcore_menu_send_unit_level_size_must_be_92[sizeof(proposition_send_unit_level_t) == 92 ? 1 : -1];

/* Provisional: the ACTIVE_PROPOSITIONS level pushed by brave_story_open_advancing_list: a cursor window and
 * three column-header windows over one render record listing each active proposition's name, participant count and
 * remaining days. */
typedef struct brave_story_advancing_level {
    s32 cursor_window; /* 0x00; WINDOW_ANIM_CURSOR_IDLE, moved to the cursor row */
    s32 title_window;  /* 0x04; WINDOW_ANIM_PROPOSITION_TITLE_LABEL */
    s32 count_window;  /* 0x08; WINDOW_ANIM_PROPOSITION_COUNT_LABEL */
    s32 days_window;   /* 0x0c; WINDOW_ANIM_PROPOSITION_DAYS_LABEL */
    s32 render_index;  /* 0x10 */
    u8 _unused_14[8];
    s32 cursor; /* 0x1c; initial row from g_brave_story_advancing_saved_cursor */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34; indices into g_proposition_active */
} brave_story_advancing_level_t;

/* The proposition detail panel's outputs, at the same offsets in the PROPOSITION_CANCEL_CONFIRM and
 * PARTICIPANT_LIST levels (proposition_build_detail_panel). */
typedef struct proposition_detail_panel {
    u8 _unused_00[8]; /* the owning level's own fields */
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
} proposition_detail_panel_t;

/* The PROPOSITION_CANCEL_CONFIRM level, which recalls a proposition. The detail-panel builder writes
 * detail_render and detail_rows; the yes/no cursor uses choice_render. */
typedef struct proposition_recall_level {
    s32 cursor_window; /* 0x00 */
    s32 choice_render; /* 0x04 */
    s32 detail_render; /* 0x08 */
    s32 detail_rows;   /* 0x0c */
    s32 proposition;   /* 0x10 */
    s32 choice;        /* 0x14 */
    s32 confirmed;     /* 0x18 */
    u8 _unused_1c[4];
    s32 frame_window; /* 0x20 */
} proposition_recall_level_t;

/* The PARTICIPANT_LIST level. The detail-panel builder fills the render index and row count before the cursor is
 * positioned. */
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

/* Provisional: the PARTICIPANT_DETAIL level pushed by brave_story_push_participant_level with two frame
 * windows; unit_panel_build_unit_data draws the participant
 * g_proposition_active[proposition].participant_indices[participant]. */
typedef struct brave_story_participant_level {
    s32 left_window;  /* 0x00 */
    s32 right_window; /* 0x04 */
    s32 _unknown_08;  /* 0x08; cleared at push */
    s32 _unused_0c;   /* 0x0c */
    s32 proposition;  /* 0x10; index into g_proposition_active */
    s32 participant;  /* 0x14; slot in participant_indices */
} brave_story_participant_level_t;

/* Provisional: the COMPLETED_PROPOSITIONS_BROWSE level, which asks whether to hear a returned proposition's report.
 * proposition_handle_returned_list_input pushes it over the yes/no panel. */
typedef struct proposition_report_confirm_level {
    s32 cursor_window; /* 0x00; window record moved between the two rows */
    s32 render_index;  /* 0x04; render record holding the confirmation text */
    s32 selection;     /* 0x08; 0 = upper row, 1 = lower row */
    s32 _unused_0c;
    s32 argument; /* 0x10; argument of the level proposition_push_report_level pushes */
} proposition_report_confirm_level_t;

/* proposition_report_level_t.load_phase. */
typedef enum proposition_report_load {
    PROPOSITION_REPORT_LOAD_DONE = 0,
    PROPOSITION_REPORT_LOAD_MESSAGES = 1, /* waits for the audio queue, then loads the message block */
    PROPOSITION_REPORT_LOAD_PICTURE = 2,  /* waits for it, then loads the reward's picture set */
} proposition_report_load_e;

/* Provisional: the PROPOSITION_REPORT level pushed by proposition_push_report_level, which starts the
 * message thread (WORLD_MAP_THREAD_SLOT_MESSAGE), and stepped by proposition_step_report_level. */
typedef struct proposition_report_level {
    s32 state; /* 0x00; step index, -1 .. 0x12; pushed as -1, or 0x63 when the proposition was recalled */
    s32 phase; /* 0x04; proposition_step_message phase; cleared at push */
    u8 _unused_08[4];
    s32 load_phase;   /* 0x0c; proposition_report_load_e; MESSAGES at push */
    s32 render_index; /* 0x10; picture render record; -1 at push */
    u8 _unused_14[0x18];
    s32 proposition; /* 0x2c; argument of the push */
} proposition_report_level_t;

/* proposition_result_level_t.phase. */
typedef enum proposition_result_phase {
    PROPOSITION_RESULT_PHASE_INPUT = 0,
    PROPOSITION_RESULT_PHASE_CLOSING = 1, /* waits for the audio queue and the message thread */
    PROPOSITION_RESULT_PHASE_DONE = 2,    /* waits for the audio queue, then pops */
} proposition_result_phase_e;

/* Provisional: the PROPOSITION_RESULT level pushed by proposition_push_result_level and stepped by
 * proposition_step_result_level. */
typedef struct proposition_result_level {
    s32 render_index; /* 0x00 */
    u8 _unused_04[0xc];
    s32 phase; /* 0x10; proposition_result_phase_e */
    u8 _unused_14[0x14];
    s32 delay;       /* 0x28; frames before the success jingle restarts */
    s32 proposition; /* 0x2c; index into g_proposition_active */
    u8 _unused_30[0x2c];
} proposition_result_level_t;

/* sound novels */
/* The SOUND_NOVEL_RESUME level pushed by sound_novel_push_restart_level and stepped by
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

/* sound_novel_level_t.phase. */
typedef enum sound_novel_phase {
    SOUND_NOVEL_PHASE_RUN = 0,          /* the script runs */
    SOUND_NOVEL_PHASE_OPEN_HISTORY = 2, /* square: counts down, then pushes the history level */
    SOUND_NOVEL_PHASE_END = 3,          /* the script ended: waits for thread 14, then leaves */
} sound_novel_phase_e;

/* The SOUND_NOVEL level pushed by sound_novel_push_level and stepped by
 * sound_novel_step_level while the script runs: the Start and Square button prompts, and the
 * brightness bar with its overlay, shown for a while after L1 or R1 changes the brightness. */
typedef struct sound_novel_level {
    s32 left_window;    /* 0x00; the Start button prompt (the Quit/Continue menu) */
    s32 right_window;   /* 0x04; the Square button prompt (the text history) */
    s32 render_index;   /* 0x08; the brightness bar, a VRAM strip render record */
    s32 overlay_window; /* 0x0c; the brightness bar's overlay */
    u8 _unused_10[0xc];
    s32 message;     /* 0x1c; the treasure detail reopened when the novel ends */
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

/* sound_novel_history_level_t.phase. */
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

/* the stack record */
/* Provisional: one 0x5c-byte slot of the menu stack (g_menu_stack_records), one per level; the level's type
 * in g_menu_stack_types selects the view. The leading 32-bit value normally names a window; the
 * FOCUS_LOCATION level stores a map-dot index instead. */
typedef union menu_stack_record {
    s32 window_index;
    s32 focus_dot_index;   /* MENU_STACK_LEVEL_WORLD_MAP_FOCUS */
    s32 story_event_index; /* MENU_STACK_LEVEL_BRAVE_STORY_EVENT */
    unit_panel_help_level_t layout_window;
    menu_stack_message_level_t message;
    world_map_menu_level_t return_level;
    menu_stack_panel_level_t panel;
    window_render_pair_t window_render;
    window_pair_level_t window_pair;
    window_pair_render_level_t window_pair_render;
    proposition_discovery_level_t discovery; /* pushed by proposition_push_discovery_level */
    rumor_detail_level_t rumor_detail;
    menu_stack_window_level_t list_window;
    brave_story_advancing_level_t proposition_list;
    debug_variable_list_level_t variable_list;
    brave_story_participant_level_t participant;
    brave_story_participant_list_level_t participant_list;
    proposition_recall_level_t proposition_cancel_confirm;
    proposition_detail_panel_t proposition_detail_panel;
    brave_story_detail_level_t detail;
    event_transition_level_t event_transition;
    sound_novel_restart_level_t sound_novel_resume;
    screen_transition_level_t screen_transition;
    menu_stack_scrollable_text_level_t scrollable_text;
    sound_novel_quit_level_t sound_novel_quit;
    sound_novel_history_level_t sound_novel_history;
    travel_level_t location_transition;
    proposition_send_unit_level_t send_unit;
    proposition_confirm_level_t choice_panel;
    proposition_report_confirm_level_t proposition_report_confirm;
    tutorial_menu_panel_t text_list;
    world_map_cursor_level_t map_cursor;
    world_map_visibility_level_t visibility;
    world_map_path_level_t path;
    location_menu_state_t location_menu;
    location_menu_level_t location_list;
    proposition_report_level_t proposition_report;
    sound_novel_level_t sound_novel;
    proposition_result_level_t proposition_result;
} menu_stack_record_t;
typedef char wldcore_menu_stack_record_size_must_be_92[sizeof(menu_stack_record_t) == 92 ? 1 : -1];

#endif
