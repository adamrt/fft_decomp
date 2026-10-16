#ifndef WLDCORE_MENU_LEVEL_H
#define WLDCORE_MENU_LEVEL_H

/* WLDCORE's menu levels: the level types, the record each push routine fills in its 0x5c-byte slot of the menu stack
 * (grouped by the screens that push them), and the union the stack keeps them in. */

#include "psx/types.h"

/* level types */
/* The type of a menu level: it selects the level's idle, publish and resume handlers. Push routines fill the level
 * record; RETURN bridges the entry window and a pending world-map transition. */
typedef enum wldcore_menu_level_type {
    WLDCORE_MENU_LEVEL_ROOT = 0,
    WLDCORE_MENU_LEVEL_RETURN = 1,
    WLDCORE_MENU_LEVEL_TEXT_WINDOW = 2,
    WLDCORE_MENU_LEVEL_MESSAGE = 3,
    WLDCORE_MENU_LEVEL_LOCATION_MENU = 4,
    WLDCORE_MENU_LEVEL_BAR_MENU = 5,
    WLDCORE_MENU_LEVEL_RUMORS = 6,
    WLDCORE_MENU_LEVEL_AVAILABLE_PROPOSITIONS = 7,
    WLDCORE_MENU_LEVEL_RUMOR_DETAIL = 8,
    WLDCORE_MENU_LEVEL_PROPOSITION_DETAIL = 9,
    WLDCORE_MENU_LEVEL_PROPOSITION_ACCEPT = 10,
    WLDCORE_MENU_LEVEL_VARIABLE_LIST = 11,
    WLDCORE_MENU_LEVEL_NUMBER_PANEL = 12,
    WLDCORE_MENU_LEVEL_PROPOSITION_DISPATCH_CONFIRM = 13,
    WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS = 14,
    WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS_BROWSE = 15,
    WLDCORE_MENU_LEVEL_PROPOSITION_REPORT = 16,
    WLDCORE_MENU_LEVEL_PROPOSITION_RESULT = 17,
    WLDCORE_MENU_LEVEL_PROPOSITIONS = 18,
    WLDCORE_MENU_LEVEL_PROPOSITION_CANCEL_CONFIRM = 19,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_RESUME = 20,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL = 21,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_QUIT = 22,
    WLDCORE_MENU_LEVEL_SOUND_NOVEL_HISTORY = 23,
    WLDCORE_MENU_LEVEL_UNEXPLORED_LANDS = 24,
    WLDCORE_MENU_LEVEL_UNEXPLORED_LAND_DETAIL = 25,
    WLDCORE_MENU_LEVEL_TREASURES = 26,
    WLDCORE_MENU_LEVEL_TREASURE_DETAIL = 27,
    WLDCORE_MENU_LEVEL_BRAVE_STORY = 28,
    WLDCORE_MENU_LEVEL_STORY_EVENTS = 29,
    WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT = 30,
    WLDCORE_MENU_LEVEL_PERSONS = 31,
    WLDCORE_MENU_LEVEL_VARIABLE_DETAIL = 32,
    WLDCORE_MENU_LEVEL_TUTORIAL_TOPICS = 33,
    WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT = 34,
    WLDCORE_MENU_LEVEL_EVENT_TRANSITION = 35,
    WLDCORE_MENU_LEVEL_PROPOSITION_SEND_UNIT = 36,
    WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY = 37,
    WLDCORE_MENU_LEVEL_ACTIVE_PROPOSITIONS = 38,
    WLDCORE_MENU_LEVEL_PARTICIPANT_LIST = 39,
    WLDCORE_MENU_LEVEL_PARTICIPANT_DETAIL = 40,
    WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS = 41,
    WLDCORE_MENU_LEVEL_PROPOSITION_ATTEMPT_DETAIL = 42,
    WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES = 43,
    WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS = 44,
    WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES = 45,
    WLDCORE_MENU_LEVEL_FACTS = 46,
    WLDCORE_MENU_LEVEL_FACT_DETAIL = 47,
    WLDCORE_MENU_LEVEL_NUMBER_PANEL_WITH_ARGUMENT = 48,
    WLDCORE_MENU_LEVEL_SCREEN_TRANSITION = 49,
    WLDCORE_MENU_LEVEL_FOCUS_LOCATION = 50,
    WLDCORE_MENU_LEVEL_SCROLLABLE_TEXT_WINDOW = 51,
    WLDCORE_MENU_LEVEL_FIXED_NUMBER_PANEL = 52,
    WLDCORE_MENU_LEVEL_MAP_PATH = 53,
    WLDCORE_MENU_LEVEL_MAP_LOCATION_VISIBILITY = 54,
    WLDCORE_MENU_LEVEL_COUNTDOWN = 55,
    WLDCORE_MENU_LEVEL_LAYOUT_WINDOW = 56,
} wldcore_menu_level_type_e;

/* shared level parts */
/* A window record and a render record handled together (wldcore_window_set_render_palette_2). */
typedef struct wldcore_window_render_pair {
    s32 window_index;
    s32 render_index;
} wldcore_window_render_pair_t;

/* Leading 32-bit fields of a menu level that owns up to two window/render record
 * pairs; wldcore_menu_close_level_and_release_records releases each index
 * that is not -1. */
typedef struct wldcore_menu_slot_set {
    s32 result;   /* 0x00; the location menu step sets 1 on confirm, 0 on cancel */
    s32 window_a; /* 0x04 */
    s32 render_a; /* 0x08 */
    s32 window_b; /* 0x0c */
    s32 render_b; /* 0x10 */
} wldcore_menu_slot_set_t;

/* The window-layout request that opens a LAYOUT_WINDOW level. wldcore_window_apply_layout reads the first three
 * fields. */
typedef struct wldcore_window_layout_request {
    s32 window_index;
    s32 layout; /* 0x04; row of g_wldcore_window_layout_rows */
    s32 cached_layout_kind;
    s32 saved_state; /* 0x0c; previous g_wldcore_menu_ordering_table_offset, saved by the push */
} wldcore_window_layout_request_t;

/* entry, messages and text windows */
/* Provisional: the RETURN level that closes the world map, pushed by wldcore_menu_push_entry_window_level.
 * wldcore_menu_step_return_level is its only reader: delay counts down the frames the fade needs and pending latches
 * while MAIN_SYSTEM_FLAG_SCREEN_CLEARED is up. */
typedef struct wldcore_menu_return_level {
    s32 delay;
    s32 pending; /* 0x04 */
} wldcore_menu_return_level_t;

/* The MESSAGE and TEXT_WINDOW levels (wldcore_menu_push_message_level, wldcore_menu_push_text_window_level). */
typedef struct wldcore_menu_message_level {
    s32 reload_text;
    s32 saved_blocked_buttons; /* 0x04; g_menu_blocked_buttons at push time */
    s32 window_index;          /* 0x08; window record appended by the TEXT_WINDOW push; cleared by the MESSAGE push */
    s32 _unknown_0c;           /* 0x0c; set to 8 at push */
} wldcore_menu_message_level_t;

/* The SCROLLABLE_TEXT_WINDOW level, pushed by wldcore_menu_push_scrollable_text_window_level and stepped by
 * wldcore_menu_step_scrollable_text_window_level. */
typedef struct wldcore_menu_scrollable_text_level {
    s32 window_index; /* 0x00; window record of the frame */
    u8 _unused_04[0x1c];
    s32 _unknown_20; /* 0x20; 0x28 at push; nothing reads it */
} wldcore_menu_scrollable_text_level_t;

/* A level owning two window records; wldcore_opcode_set_parent_windows_visible toggles both records'
 * WORLD_RENDER_RECORD_HIDDEN. */
typedef struct wldcore_menu_window_pair_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
} wldcore_menu_window_pair_level_t;

/* A level owning two window records and a render record: wldcore_bar_push_menu_level pushes it as BAR_MENU.
 * wldcore_menu_push_tutorial_topic_text_level hides all three (WORLD_RENDER_RECORD_HIDDEN) and
 * wldcore_menu_wait_tutorial_topic_text_then_pop_level shows them again. */
typedef struct wldcore_menu_window_pair_render_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 panel_rows;    /* 0x0c; cleared by wldcore_bar_push_menu_level */
} wldcore_menu_window_pair_render_level_t;

/* Provisional: owner of a text-list render record, as
 * wldcore_list_build_tutorial_category_panel_image reads it; only the record
 * index, row count and text-id rows are established. */
typedef struct wldcore_text_list_panel {
    u8 _unused_00[8];
    s32 render_index; /* 0x08 */
    u8 _unused_0c[0x24];
    s32 row_count;   /* 0x30 */
    s32 text_ids[1]; /* 0x34; row_count entries, relative to WLDCORE_TEXT_ID_TUTORIAL_CATEGORY_ROW_FIRST */
} wldcore_text_list_panel_t;

/* lists, panels and details */
/* Panel levels: the number panels (wldcore_menu_push_number_panel_level and its _with_argument form) and the list
 * panels (wldcore_list_open_unexplored_lands, wldcore_list_open_tutorial_topics and
 * wldcore_menu_pop_level_and_rebuild_screen). */
typedef struct wldcore_menu_panel_level {
    /* 0x00; number panel: 0 running, 1 confirmed, 2 canceled; list panels
     * also use -1 for cancellation. Read after the panel thread (WLDCORE_THREAD_SLOT_WINDOW_PANEL) ends. */
    s32 result;
    s32 variable_id; /* 0x04; script variable edited by number panels */
    u8 _unused_08[0x28];
    s32 entry_count; /* 0x30; rows in the panel list */
    /* 0x34; entry text ids, filled by wldcore_map_build_location_menu_entries, whose result is the entry count;
     * the same offset as the list_window and proposition_list entries */
    s32 entries[10];
} wldcore_menu_panel_level_t;

/* Phases of a list-window level (wldcore_menu_list_window_level_t.mode). The
 * town menu uses only the first two. */
typedef enum wldcore_list_window_mode {
    WLDCORE_LIST_WINDOW_MODE_BROWSE = 0,
    WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE = 1, /* the farewell text is up; waits for circle or cross */
    WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP = 2,  /* fading out after the farewell, or under the help message */
    WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT = 3,    /* faded out; waits for thread 14 to end, then pops */
} wldcore_list_window_mode_e;

/* Provisional: the list-window level of the bar, the Brave Story and the tutorials, beneath their flag-list panels.
 * wldcore_list_open_brave_story_propositions and wldcore_list_open_persons hide its content render record when they
 * push; wldcore_menu_close_fact_detail_level shows its upper and lower windows again when it pops back to it. */
typedef struct wldcore_menu_list_window_level {
    s32 main_window;
    s32 side_window;    /* 0x04 */
    s32 frame_render;   /* 0x08 */
    s32 content_render; /* 0x0c */
    u8 _unused_10[8];
    s32 mode;           /* 0x18; wldcore_list_window_mode_e */
    s32 selected_entry; /* 0x1c; initial row from g_wldcore_brave_story_saved_cursor */
    s32 upper_window;   /* 0x20; Brave Story: completion star of the unexplored lands row */
    s32 lower_window;   /* 0x24; Brave Story: completion star of the treasures row */
    u8 _unused_28[8];
    s32 entry_count; /* 0x30; rows drawn by wldcore_menu_push_brave_story_level */
    s32 entries[10]; /* 0x34; row kinds of the owning list (wldcore_*_row_e); Brave Story text is
                      * WLDCORE_TEXT_ID_BRAVE_STORY_ROW_FIRST + kind */
} wldcore_menu_list_window_level_t;

/* Provisional: the debug VARIABLE_LIST level pushed by wldcore_menu_push_variable_list_level;
 * wldcore_menu_render_debug_variable_list draws row_count variables from first_variable into the render record. */
typedef struct wldcore_menu_variable_list_level {
    s32 window_index;   /* 0x00; cursor window */
    s32 render_index;   /* 0x04 */
    s32 first_variable; /* 0x08 */
    s32 cursor;         /* 0x0c; cursor row */
    u8 _unused_10[0x20];
    s32 row_count; /* 0x30 */
} wldcore_menu_variable_list_level_t;

/* wldcore_menu_variable_detail_level_t.phase, shared by the variable,
 * treasure and unexplored-land detail steps. */
typedef enum wldcore_detail_phase {
    WLDCORE_DETAIL_PHASE_SHOWN = 0,
    WLDCORE_DETAIL_PHASE_LOAD = 1,   /* waits for the file */
    WLDCORE_DETAIL_PHASE_BUILD = 2,  /* waits for the message box, then builds the text and picture */
    WLDCORE_DETAIL_PHASE_POP_IN = 3, /* waits for the text window's opening animation */
} wldcore_detail_phase_e;

/* Provisional: the VARIABLE_DETAIL level pushed by wldcore_menu_push_variable_detail_level over the list-window
 * level; it keeps the selected entry value (variable index * 16 + value) and the render record it appends. The
 * treasure and unexplored-land details use the same record. */
typedef struct wldcore_menu_variable_detail_level {
    s32 value;        /* 0x00 */
    s32 render_index; /* 0x04 */
    /* 0x08; no push sets it. The treasure detail stores the saved record of the treasure's sound novel
     * here before it leaves. The treasure and unexplored-land details also read it in their LOAD phase,
     * as a render record index, when it still holds what the last level at this depth left. */
    s32 saved_record_index;
    s32 phase; /* 0x0c; wldcore_detail_phase_e; LOAD at push */
    s32 timer; /* 0x10; cleared at push; text delay or dissolve step, counted per frame */
} wldcore_menu_variable_detail_level_t;

/* transitions and countdowns */
/* Provisional: the SCREEN_TRANSITION level pushed by wldcore_menu_push_screen_transition_level and stepped by
 * wldcore_menu_step_screen_transition_level. */
typedef struct wldcore_menu_screen_transition_level {
    s32 screen;     /* 0x00; wldcore_menu_level_type_e of the level to open */
    s32 param_a;    /* 0x04 */
    s32 param_b;    /* 0x08 */
    s32 _unused_0c; /* 0x0c */
    s32 delay;      /* 0x10; 0x14 at push */
    s32 phase;      /* 0x14; cleared at push */
} wldcore_menu_screen_transition_level_t;

/* wldcore_menu_event_transition_level_t.mode, the argument of
 * wldcore_menu_push_event_transition_level and the second operand of the
 * world script's load-event instruction. Any value but 0 and 2 acts as 1. */
typedef enum wldcore_event_transition_mode {
    WLDCORE_EVENT_TRANSITION_MODE_SPIN = 0, /* spins the screen in and back out: a random battle */
    /* Spins the screen in with a second sound and whites it out: every
     * battle the world scripts start. */
    WLDCORE_EVENT_TRANSITION_MODE_SPIN_IN = 1,
    WLDCORE_EVENT_TRANSITION_MODE_FADE = 2, /* a plain screen fade: every scene they start */
} wldcore_event_transition_mode_e;

/* wldcore_menu_event_transition_level_t.phase. */
typedef enum wldcore_event_transition_phase {
    WLDCORE_EVENT_TRANSITION_PHASE_SPIN_IN = 0,
    WLDCORE_EVENT_TRANSITION_PHASE_SPIN_OUT = 1,
    WLDCORE_EVENT_TRANSITION_PHASE_DONE = 2,
} wldcore_event_transition_phase_e;

/* wldcore_menu_event_transition_level_t.fade_phase. */
typedef enum wldcore_event_transition_fade {
    WLDCORE_EVENT_TRANSITION_FADE_NONE = 0,
    WLDCORE_EVENT_TRANSITION_FADE_FLASH = 1,     /* the fill box ramps by 0x10 up to 0x80, then restarts as WHITE_OUT */
    WLDCORE_EVENT_TRANSITION_FADE_WHITE_OUT = 2, /* ramps by 8 to white, then leaves the world loop */
} wldcore_event_transition_fade_e;

/* wldcore_menu_event_transition_level_t.drift_direction. */
typedef enum wldcore_event_transition_drift {
    WLDCORE_EVENT_TRANSITION_DRIFT_RIGHT = 0x01, /* clear: left */
    WLDCORE_EVENT_TRANSITION_DRIFT_DOWN = 0x02,  /* clear: up */
} wldcore_event_transition_drift_e;

/* Provisional: the EVENT_TRANSITION level pushed by wldcore_menu_push_event_transition_level and stepped by
 * wldcore_menu_step_event_transition_level, which spins or fades the screen out before an event.
 * wldcore_draw_close_indicator_if_pending draws g_wldcore_menu_indicator_sprite while the parent level's mode is
 * not 2. */
typedef struct wldcore_menu_event_transition_level {
    s32 frame;           /* 0x00; counts step frames; the sprite updates every third, 1 restores the captured screen */
    s32 drift_direction; /* 0x04; wldcore_event_transition_drift_e; rand() * 4 >> 15 at push */
    s32 spin_direction;  /* 0x08; rand() * 2 >> 15 at push; selects +/-0x2000 rotation per update */
    s32 phase;           /* 0x0c; wldcore_event_transition_phase_e */
    s32 fade_phase;      /* 0x10; wldcore_event_transition_fade_e */
    s32 anim_step;       /* 0x14; +2/+4 per update; drives scale, shade, sounds 0x6c/0x6d and phase changes */
    u8 _unused_18[8];
    s32 mode; /* 0x20; wldcore_event_transition_mode_e; argument of the push */
} wldcore_menu_event_transition_level_t;

/* wldcore_menu_countdown_t.phase. */
typedef enum wldcore_countdown_phase {
    WLDCORE_COUNTDOWN_PHASE_MESSAGES = 0,     /* circle shows the next message */
    WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE = 1, /* waits for the message box, then starts MAIN_MUSIC_MILESTONE */
    WLDCORE_COUNTDOWN_PHASE_JINGLE = 2,       /* 180 frames, then back to MAIN_MUSIC_BAR */
    WLDCORE_COUNTDOWN_PHASE_CLOSE = 3,        /* waits for the audio queue, then pops */
} wldcore_countdown_phase_e;

/* Provisional: the COUNTDOWN level pushed by wldcore_menu_push_countdown_level: a run of messages ending in a
 * jingle. wldcore_menu_show_next_sequence_message shows each message and sets LAST_MESSAGE when the count runs
 * out; wldcore_map_step_discovery_announcement then advances the later phases. */
typedef struct wldcore_menu_countdown {
    s32 current;
    s32 remaining;
    s32 phase; /* 0x08; wldcore_countdown_phase_e */
    s32 timer; /* 0x0c; frames spent in phase 2 */
} wldcore_menu_countdown_t;

/* map and locations */
/* Provisional: the world-map cursor level (ROOT) stepped by wldcore_menu_step_map_cursor_level. */
typedef struct wldcore_menu_map_cursor_level {
    s32 window_index;     /* 0x00; the map cursor window */
    s32 alt_window_index; /* 0x04; the wide-view window swapped in by 0x80 */
    s32 wide_view;        /* 0x08; non-zero while the wide view is shown */
    s32 snap_hit;         /* 0x0c; dot index returned by the snap probe */
    s32 pending_mode;     /* 0x10 */
} wldcore_menu_map_cursor_level_t;

/* The MAP_LOCATION_VISIBILITY level pushed by wldcore_push_map_location_visibility_level and stepped by
 * wldcore_map_step_location_visibility_change; only the first four fields are known. */
typedef struct wldcore_map_visibility_level {
    s32 dot_index;     /* 0x00; map dot, and script variable 0x200 + dot_index */
    s32 was_set;       /* 0x04; whether the variable was already set */
    s32 delay;         /* 0x08; frames the step handler waits, 0x10 here */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} wldcore_map_visibility_level_t;
typedef char wldcore_map_visibility_level_size_must_be_92[sizeof(wldcore_map_visibility_level_t) == 92 ? 1 : -1];

/* Provisional: the MAP_PATH level pushed by wldcore_menu_push_map_path_level, decoded from the four fields it
 * writes. Its step handler wldcore_menu_step_map_path_level reads only the last two. The first two are the script's
 * path arguments, which wldcore_menu_dispatch_pending_script_requests passes from g_wldcore_script_state.args. Only
 * the first four fields of the slot are known. */
typedef struct wldcore_map_path_level {
    s32 path_id;       /* 0x00; g_wldcore_script_state.args[0] */
    s32 path_id_hi;    /* 0x04; g_wldcore_script_state.args[1] */
    s32 erase;         /* 0x08; step handler walks the path backwards when set */
    s32 pending_sound; /* 0x0c; set so the step handler plays a sound once */
    u8 _unused_10[0x4c];
} wldcore_map_path_level_t;
typedef char wldcore_map_path_level_size_must_be_92[sizeof(wldcore_map_path_level_t) == 92 ? 1 : -1];

/* Provisional: the location menu level as its step handler
 * wldcore_menu_step_location_menu_level sees it. */
typedef struct wldcore_menu_location_menu_level {
    wldcore_menu_slot_set_t slots; /* 0x00; slots.result is the confirmed choice */
    s32 delay;                     /* 0x14 */
    s32 fade;                      /* 0x18 */
    u8 _unused_1c[0x14];
    s32 entry_count; /* 0x30 */
} wldcore_menu_location_menu_level_t;

/* Provisional: the LOCATION_MENU level as wldcore_menu_push_location_menu_level fills it (the step handler's view
 * is wldcore_menu_location_menu_level_t). */
typedef struct wldcore_location_list_level {
    s32 window_index;  /* 0x00 */
    s32 cursor_window; /* 0x04; -1 until the cursor record is appended */
    s32 frame_render;  /* 0x08; -1 when there is no picture */
    s32 label_window;  /* 0x0c; -1 when there is no picture */
    s32 extra_render;  /* 0x10; -1 at push; released on close when set (wldcore_menu_slot_set_t.render_b) */
    s32 delay; /* 0x14; 8 at push, counted down by the location step (wldcore_menu_location_menu_level_t.delay) */
    s32 mode;  /* 0x18; 0 then 8 */
    s32 selected_entry;
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34 */
} wldcore_location_list_level_t;

/* Provisional: the level a route travel runs under. wldcore_map_step_route_travel uses the leading window index,
 * the location id, the pending result, and the choice text ids it collects when the travel ends. */
typedef struct wldcore_menu_location_transition_level {
    s32 window_index; /* 0x00 */
    u8 _unused_04[8];
    s32 location_id; /* 0x0c */
    u8 _unused_10[0x20];
    s32 pending_result;      /* 0x30 */
    s32 choice_text_ids[10]; /* 0x34; retained as location_list.entries after arrival */
} wldcore_menu_location_transition_level_t;
typedef char
    wldcore_menu_location_transition_level_size_must_be_92[sizeof(wldcore_menu_location_transition_level_t) == 92 ? 1
                                                                                                                  : -1];

/* bar and propositions */
/* The RUMOR_DETAIL level pushed by wldcore_bar_push_rumor_detail_level. Its close handler
 * wldcore_bar_wait_rumor_text_then_pop_level reads the saved parent window and render indices. */
typedef struct wldcore_menu_rumor_detail_level {
    s32 rumor_index;
    s32 _unused_04;
    s32 first_window;
    s32 second_window;
    s32 render_index;
} wldcore_menu_rumor_detail_level_t;

/* wldcore_menu_choice_panel_level_t.mode as wldcore_proposition_handle_accept_confirm_input steps it. */
typedef enum wldcore_accept_confirm_mode {
    WLDCORE_ACCEPT_CONFIRM_MODE_CHOOSE = 0,
    WLDCORE_ACCEPT_CONFIRM_MODE_NO_UNIT_MESSAGE = 1, /* nobody can go: a press shows the second message */
    WLDCORE_ACCEPT_CONFIRM_MODE_CLOSE_MESSAGE = 3,   /* a refusal message is up: a press closes the panel */
} wldcore_accept_confirm_mode_e;

/* The yes/no panel built by wldcore_window_build_yes_no_panel and stepped by
 * wldcore_proposition_handle_accept_confirm_input and wldcore_proposition_handle_dispatch_confirm_input. */
typedef struct wldcore_menu_choice_panel_level {
    s32 cursor_window; /* 0x00 */
    s32 render_index;  /* 0x04 */
    s32 row;           /* 0x08; 0 = yes, 1 = no */
    s32 mode;          /* 0x0c; wldcore_accept_confirm_mode_e */
    s32 header_window; /* 0x10 */
} wldcore_menu_choice_panel_level_t;

/* wldcore_menu_send_unit_level_t.mode. */
typedef enum wldcore_send_unit_mode {
    WLDCORE_SEND_UNIT_MODE_SELECT = 0,
    WLDCORE_SEND_UNIT_MODE_CANCEL = 1,       /* cross: closes back to the available propositions */
    WLDCORE_SEND_UNIT_MODE_REPLACE = 2,      /* three are chosen: picks the slot the unit replaces */
    WLDCORE_SEND_UNIT_MODE_STATUS_OPEN = 3,  /* square: waits for the list threads, then opens the status panels */
    WLDCORE_SEND_UNIT_MODE_STATUS = 4,       /* unit status, browsed with L1 and R1 */
    WLDCORE_SEND_UNIT_MODE_STATUS_CLOSE = 5, /* waits for the status threads, then reopens the list */
    WLDCORE_SEND_UNIT_MODE_ACCEPT = 6,       /* start: closes and moves on to the day count */
} wldcore_send_unit_mode_e;

/* Provisional: the PROPOSITION_SEND_UNIT level, where up to three units are chosen for a proposition.
 * wldcore_proposition_step_send_unit_slot_input selects the slot and owns the portrait render records. */
typedef struct wldcore_menu_send_unit_level {
    wldcore_window_render_pair_t list_frame; /* 0x00 */
    s32 frame_window_a;                      /* 0x08 */
    s32 frame_window_b;                      /* 0x0c */
    s32 preview_render;                      /* 0x10 */
    s32 list_render;                         /* 0x14 */
    s32 cursor_window;                       /* 0x18 */
    u8 _unused_1c[4];
    s32 mode;          /* 0x20; wldcore_send_unit_mode_e */
    s32 shown_preview; /* 0x24 */
    s32 slot;          /* 0x28 */
    u8 _unused_2c[4];
    s32 unit_count;      /* 0x30; wrap bound, written by wldcore_proposition_load_send_unit_candidates */
    s32 portrait_render; /* 0x34 */
    s32 portrait_frame;  /* 0x38 */
    s32 portrait_window; /* 0x3c */
    s32 left_arrow;      /* 0x40 */
    s32 right_arrow;     /* 0x44 */
    u8 _unused_48[0x10];
    s32 shown_unit; /* 0x58 */
} wldcore_menu_send_unit_level_t;
typedef char wldcore_menu_send_unit_level_size_must_be_92[sizeof(wldcore_menu_send_unit_level_t) == 92 ? 1 : -1];

/* Provisional: the ACTIVE_PROPOSITIONS level pushed by wldcore_list_open_active_propositions: a cursor window and
 * three column-header windows over one render record listing each active proposition's name, participant count and
 * remaining days. */
typedef struct wldcore_menu_proposition_list_level {
    s32 cursor_window; /* 0x00; WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE, moved to the cursor row */
    s32 title_window;  /* 0x04; WLDCORE_ANIM_SEQUENCE_PROPOSITION_TITLE_LABEL */
    s32 count_window;  /* 0x08; WLDCORE_ANIM_SEQUENCE_PROPOSITION_COUNT_LABEL */
    s32 days_window;   /* 0x0c; WLDCORE_ANIM_SEQUENCE_PROPOSITION_DAYS_LABEL */
    s32 render_index;  /* 0x10 */
    u8 _unused_14[8];
    s32 cursor; /* 0x1c; initial row from g_wldcore_active_propositions_saved_cursor */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
    s32 entries[10]; /* 0x34; indices into g_main_active_propositions */
} wldcore_menu_proposition_list_level_t;

/* The proposition detail panel's outputs, at the same offsets in the PROPOSITION_CANCEL_CONFIRM and
 * PARTICIPANT_LIST levels (wldcore_list_build_proposition_detail_panel_image). */
typedef struct wldcore_menu_proposition_detail_panel {
    u8 _unused_00[8]; /* the owning level's own fields */
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
} wldcore_menu_proposition_detail_panel_t;

/* The PROPOSITION_CANCEL_CONFIRM level, which recalls a proposition. The detail-panel builder writes
 * detail_render and detail_rows; the yes/no cursor uses choice_render. */
typedef struct wldcore_menu_proposition_cancel_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 choice_render; /* 0x04 */
    s32 detail_render; /* 0x08 */
    s32 detail_rows;   /* 0x0c */
    s32 proposition;   /* 0x10 */
    s32 choice;        /* 0x14 */
    s32 confirmed;     /* 0x18 */
    u8 _unused_1c[4];
    s32 frame_window; /* 0x20 */
} wldcore_menu_proposition_cancel_confirm_level_t;

/* The PARTICIPANT_LIST level. The detail-panel builder fills the render index and row count before the cursor is
 * positioned. */
typedef struct wldcore_menu_participant_list_level {
    s32 cursor_window; /* 0x00 */
    u8 _unused_04[4];
    s32 render_index; /* 0x08 */
    s32 panel_rows;   /* 0x0c */
    s32 proposition;  /* 0x10 */
    u8 _unused_14[4];
    s32 mode;   /* 0x18; set to WLDCORE_LIST_WINDOW_MODE_BROWSE */
    s32 cursor; /* 0x1c */
    u8 _unused_20[0x10];
    s32 entry_count; /* 0x30 */
} wldcore_menu_participant_list_level_t;

/* Provisional: the PARTICIPANT_DETAIL level pushed by wldcore_menu_push_participant_detail_level with two frame
 * windows; wldcore_unit_build_status_panel_data draws the participant
 * g_main_active_propositions[proposition].participant_indices[participant]. */
typedef struct wldcore_menu_participant_level {
    s32 left_window;  /* 0x00 */
    s32 right_window; /* 0x04 */
    s32 _unknown_08;  /* 0x08; cleared at push */
    s32 _unused_0c;   /* 0x0c */
    s32 proposition;  /* 0x10; index into g_main_active_propositions */
    s32 participant;  /* 0x14; slot in participant_indices */
} wldcore_menu_participant_level_t;

/* Provisional: the COMPLETED_PROPOSITIONS_BROWSE level, which asks whether to hear a returned proposition's report.
 * wldcore_list_handle_completed_propositions_input pushes it over the yes/no panel. */
typedef struct wldcore_proposition_report_confirm_level {
    s32 cursor_window; /* 0x00; window record moved between the two rows */
    s32 render_index;  /* 0x04; render record holding the confirmation text */
    s32 selection;     /* 0x08; 0 = upper row, 1 = lower row */
    s32 _unused_0c;
    s32 argument; /* 0x10; argument of the level wldcore_menu_push_proposition_report_level pushes */
} wldcore_proposition_report_confirm_level_t;

/* wldcore_proposition_report_level_t.load_phase. */
typedef enum wldcore_proposition_report_load {
    WLDCORE_PROPOSITION_REPORT_LOAD_DONE = 0,
    WLDCORE_PROPOSITION_REPORT_LOAD_MESSAGES = 1, /* waits for the audio queue, then loads the message block */
    WLDCORE_PROPOSITION_REPORT_LOAD_PICTURE = 2,  /* waits for it, then loads the reward's picture set */
} wldcore_proposition_report_load_e;

/* Provisional: the PROPOSITION_REPORT level pushed by wldcore_menu_push_proposition_report_level, which starts the
 * message thread (WLDCORE_THREAD_SLOT_MESSAGE), and stepped by wldcore_proposition_step_report_level. */
typedef struct wldcore_proposition_report_level {
    s32 state; /* 0x00; step index, -1 .. 0x12; pushed as -1, or 0x63 when the proposition was recalled */
    s32 phase; /* 0x04; wldcore_proposition_step_message phase; cleared at push */
    u8 _unused_08[4];
    s32 load_phase;   /* 0x0c; wldcore_proposition_report_load_e; MESSAGES at push */
    s32 render_index; /* 0x10; picture render record; -1 at push */
    u8 _unused_14[0x18];
    s32 proposition; /* 0x2c; argument of the push */
} wldcore_proposition_report_level_t;

/* wldcore_proposition_result_level_t.phase. */
typedef enum wldcore_proposition_result_phase {
    WLDCORE_PROPOSITION_RESULT_PHASE_INPUT = 0,
    WLDCORE_PROPOSITION_RESULT_PHASE_CLOSING = 1, /* waits for the audio queue and the message thread */
    WLDCORE_PROPOSITION_RESULT_PHASE_DONE = 2,    /* waits for the audio queue, then pops */
} wldcore_proposition_result_phase_e;

/* Provisional: the PROPOSITION_RESULT level pushed by wldcore_proposition_push_result_level and stepped by
 * wldcore_proposition_step_result_level. */
typedef struct wldcore_proposition_result_level {
    s32 render_index; /* 0x00 */
    u8 _unused_04[0xc];
    s32 phase; /* 0x10; wldcore_proposition_result_phase_e */
    u8 _unused_14[0x14];
    s32 delay;       /* 0x28; frames before the success jingle restarts */
    s32 proposition; /* 0x2c; index into g_main_active_propositions */
    u8 _unused_30[0x2c];
} wldcore_proposition_result_level_t;

/* sound novels */
/* The SOUND_NOVEL_RESUME level pushed by wldcore_menu_push_sound_novel_resume_level and stepped by
 * wldcore_menu_step_sound_novel_resume_level: a cursor window over the saved-picture render record and two text
 * render records. Only mode shares its meaning with the EVENT_TRANSITION level at the same offset. */
typedef struct wldcore_menu_sound_novel_resume_level {
    s32 cursor_window;  /* 0x00; moved between the two option rows */
    s32 picture_render; /* 0x04; VRAM render record of the TIM at g_wldcore_picture_buffer */
    s32 message_render; /* 0x08; WLDCORE_TEXT_ID_SUBSTITUTED_TEXT */
    s32 option_render;  /* 0x0c; WLDCORE_TEXT_ID_SOUND_NOVEL_RESUME_CHOICES; the cursor rows are relative to its base */
    s32 dissolve_step;  /* 0x10; step of the progressive picture upload, 0 once it finishes */
    s32 cursor_row;     /* 0x14; 0 or 1, toggled by up/down; 1 resets the selected saved record */
    u8 _unused_18[8];
    s32 mode; /* 0x20; argument of the push */
} wldcore_menu_sound_novel_resume_level_t;

/* wldcore_menu_sound_novel_level_t.phase. */
typedef enum wldcore_sound_novel_phase {
    WLDCORE_SOUND_NOVEL_PHASE_RUN = 0,          /* the script runs */
    WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY = 2, /* square: counts down, then pushes the history level */
    WLDCORE_SOUND_NOVEL_PHASE_END = 3,          /* the script ended: waits for thread 14, then leaves */
} wldcore_sound_novel_phase_e;

/* Provisional: the SOUND_NOVEL level pushed by wldcore_menu_push_sound_novel_level and stepped by
 * wldcore_menu_step_sound_novel_level while the script runs: title, header and footer windows around a bar render
 * record. */
typedef struct wldcore_menu_sound_novel_level {
    s32 left_window;    /* 0x00; title window */
    s32 right_window;   /* 0x04; header window */
    s32 render_index;   /* 0x08; bar render record */
    s32 overlay_window; /* 0x0c; footer window */
    u8 _unused_10[0xc];
    s32 message;     /* 0x1c; argument of the push */
    s32 _unknown_20; /* 0x20; set to 1 at push */
    s32 countdown;   /* 0x24; 9999 at push */
    s32 phase;       /* 0x28; cleared at push */
    s32 fade_timer;  /* 0x2c; cleared at push */
} wldcore_menu_sound_novel_level_t;

/* Provisional: the SOUND_NOVEL_QUIT level, which asks whether to save the reading before leaving; pushed by
 * wldcore_menu_push_sound_novel_quit_level and stepped by wldcore_menu_step_sound_novel_quit_level. */
typedef struct wldcore_menu_save_confirm_level {
    s32 cursor_window; /* 0x00 */
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 _unused_0c;
    s32 fade_out;    /* 0x10 */
    s32 fade_in;     /* 0x14 */
    s32 _unknown_18; /* 0x18; cleared at push */
    s32 choice;      /* 0x1c */
} wldcore_menu_save_confirm_level_t;

/* wldcore_menu_sound_novel_history_level_t.phase. */
typedef enum wldcore_sound_novel_history_phase {
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE = 0,
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM = 1,     /* five steps: the picture darkens under the fill box */
    WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE = 2, /* the reverse ramp, then the level pops */
} wldcore_sound_novel_history_phase_e;

/* Provisional: the SOUND_NOVEL_HISTORY level pushed by wldcore_menu_push_sound_novel_history_level, which stores
 * the newest history text, the three older ones, phase DIM and step 0. The draw helper reads the same record. */
typedef struct wldcore_menu_sound_novel_history_level {
    s32 upper_window;   /* 0x00 */
    s32 lower_window;   /* 0x04 */
    s32 render_index;   /* 0x08 */
    s32 newest_text_id; /* 0x0c; newest history text */
    s32 text_ids[3];    /* 0x10 */
    s32 scroll;         /* 0x1c */
    s32 phase;          /* 0x20; wldcore_sound_novel_history_phase_e */
    s32 step;           /* 0x24; 0..4 fade step */
} wldcore_menu_sound_novel_history_level_t;

/* the stack record */
/* Provisional: one 0x5c-byte slot of the menu stack (g_wldcore_menu_stack_records), one per level; the level's type
 * in g_wldcore_menu_stack_types selects the view. The leading 32-bit value normally names a window; the
 * FOCUS_LOCATION level stores a map-dot index instead. */
typedef union wldcore_menu_stack_record {
    s32 window_index;
    s32 focus_dot_index;   /* WLDCORE_MENU_LEVEL_FOCUS_LOCATION */
    s32 story_event_index; /* WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT */
    wldcore_window_layout_request_t layout_window;
    wldcore_menu_message_level_t message;
    wldcore_menu_return_level_t return_level;
    wldcore_menu_panel_level_t panel;
    wldcore_window_render_pair_t window_render;
    wldcore_menu_window_pair_level_t window_pair;
    wldcore_menu_window_pair_render_level_t window_pair_render;
    wldcore_menu_countdown_t countdown; /* pushed by wldcore_menu_push_countdown_level */
    wldcore_menu_rumor_detail_level_t rumor_detail;
    wldcore_menu_list_window_level_t list_window;
    wldcore_menu_proposition_list_level_t proposition_list;
    wldcore_menu_variable_list_level_t variable_list;
    wldcore_menu_participant_level_t participant;
    wldcore_menu_participant_list_level_t participant_list;
    wldcore_menu_proposition_cancel_confirm_level_t proposition_cancel_confirm;
    wldcore_menu_proposition_detail_panel_t proposition_detail_panel;
    wldcore_menu_variable_detail_level_t variable_detail;
    wldcore_menu_event_transition_level_t event_transition;
    wldcore_menu_sound_novel_resume_level_t sound_novel_resume;
    wldcore_menu_screen_transition_level_t screen_transition;
    wldcore_menu_scrollable_text_level_t scrollable_text;
    wldcore_menu_save_confirm_level_t save_confirm;
    wldcore_menu_sound_novel_history_level_t sound_novel_history;
    wldcore_menu_location_transition_level_t location_transition;
    wldcore_menu_send_unit_level_t send_unit;
    wldcore_menu_choice_panel_level_t choice_panel;
    wldcore_proposition_report_confirm_level_t proposition_report_confirm;
    wldcore_text_list_panel_t text_list;
    wldcore_menu_map_cursor_level_t map_cursor;
    wldcore_map_visibility_level_t visibility;
    wldcore_map_path_level_t path;
    wldcore_menu_location_menu_level_t location_menu;
    wldcore_location_list_level_t location_list;
    wldcore_proposition_report_level_t proposition_report;
    wldcore_menu_sound_novel_level_t sound_novel;
    wldcore_proposition_result_level_t proposition_result;
} wldcore_menu_stack_record_t;
typedef char wldcore_menu_stack_record_size_must_be_92[sizeof(wldcore_menu_stack_record_t) == 92 ? 1 : -1];

#endif
