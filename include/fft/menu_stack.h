#ifndef MENU_STACK_H
#define MENU_STACK_H

/* WLDCORE's stack of menu levels: the level types, the level records several screens share, the handler tables and push
 * routines, the remembered list cursors and the list panels' row columns. */

#include "fft/menu_selection.h"
#include "psx/types.h"

/* menu stack */
/* One level's record on the menu stack, defined in menu_stack_records.h as the union of every level record. */
typedef union menu_stack_record menu_stack_record_t;

/* The type of a menu level: it selects the level's idle, suspend and resume handlers. Push routines fill the level
 * record; WORLD_MENU bridges the entry window and a pending world-map transition. */
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

enum { MENU_STACK_CAPACITY = 8 };

/* Depth is the number of active levels: the top occupies depth - 1, and a push
 * writes slot depth before incrementing it. The root occupies physical slot 0. */
extern s32 g_menu_stack_depth;
extern s32 g_menu_stack_types[MENU_STACK_CAPACITY]; /* menu_stack_level_e of each level */
extern s32 g_menu_stack_ot_offset;

/* Handlers by level type. Idle runs the top level each frame; suspend runs just before another level opens over it
 * (list levels withhold every button, window levels dim their windows); resume undoes that when the level above
 * closes. */
void menu_stack_reset(void);
void menu_stack_dispatch_idle(void);
void menu_stack_dispatch_suspend(void);
void menu_stack_dispatch_resume(void);

/* levels */
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

/* The MESSAGE and TEXT_WINDOW levels (menu_stack_push_message_level, menu_stack_push_text_window_level). */
typedef struct menu_stack_message_level {
    s32 reload_text;
    s32 saved_blocked_buttons; /* 0x04; g_menu_blocked_buttons at push time */
    s32 window_index;          /* 0x08; window record appended by the TEXT_WINDOW push; cleared by the MESSAGE push */
    s32 _unknown_0c;           /* 0x0c; set to 8 at push */
} menu_stack_message_level_t;

/* The SCROLLABLE_TEXT level, pushed by menu_stack_push_scrollable_text_level and stepped by
 * menu_stack_step_scrollable_text_level. */
typedef struct menu_stack_scrollable_text_level {
    s32 window_index; /* 0x00; window record of the frame */
    u8 _unused_04[0x1c];
    s32 _unknown_20; /* 0x20; 0x28 at push; nothing reads it */
} menu_stack_scrollable_text_level_t;

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
    s32 entries[10]; /* 0x34; row kinds of the owning list (bar_row_e, brave_story_row_e, tutorial_menu_row_e); Brave
                      * Story text is TEXT_ID_BRAVE_STORY_ROW_FIRST + kind */
} menu_stack_window_level_t;

void menu_stack_push_message_level(s32 text, s32 reload_text);
void menu_stack_push_text_window_level(s32 text, s32 reload_text);
void menu_stack_push_scrollable_text_level(s32 text_id);

/* menu levels */
/* A window record and a render record handled together (proposition_suspend_detail_level). */
typedef struct window_render_pair {
    s32 window_index;
    s32 render_index;
} window_render_pair_t;

/* A level owning two window records; sound_novel_opcode_0c_set_parent_windows_visible toggles both records'
 * DISPLAY_OBJECT_HIDDEN. */
typedef struct window_pair_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
} window_pair_level_t;

/* A level owning two window records and a render record: bar_push_level pushes it as MENU_STACK_LEVEL_BAR.
 * tutorial_push_topic_level hides all three (DISPLAY_OBJECT_HIDDEN) and tutorial_wait_topic_then_pop shows them
 * again. */
typedef struct window_pair_render_level {
    s32 first_window;
    s32 second_window; /* 0x04 */
    s32 render_index;  /* 0x08 */
    s32 panel_rows;    /* 0x0c; cleared by bar_push_level */
} window_pair_render_level_t;

/* remembered cursors */
/* Slots of g_menu_stack_cursor_states: the remembered row of each list. */
typedef enum menu_stack_cursor_slot {
    MENU_STACK_CURSOR_SLOT_LOCATION = 0,
    MENU_STACK_CURSOR_SLOT_BAR = 1,
    MENU_STACK_CURSOR_SLOT_RUMORS = 2,
    MENU_STACK_CURSOR_SLOT_AVAILABLE_PROPOSITIONS = 3,
    MENU_STACK_CURSOR_SLOT_RETURNED = 4,
    MENU_STACK_CURSOR_SLOT_ACTIVE_PROPOSITIONS = 5,
    MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS = 7,
    MENU_STACK_CURSOR_SLOT_TREASURES = 8,
    MENU_STACK_CURSOR_SLOT_EVENTS = 10,
    MENU_STACK_CURSOR_SLOT_PERSONS = 11,
    MENU_STACK_CURSOR_SLOT_TUTORIAL_TOPICS = 12,
    MENU_STACK_CURSOR_SLOT_PARTICIPANTS = 15,
    MENU_STACK_CURSOR_SLOT_WORK_HISTORY = 16,
    MENU_STACK_CURSOR_SLOT_FORMATION_TUTORIALS = 18,
    MENU_STACK_CURSOR_SLOT_BATTLE_TUTORIALS = 19,
    MENU_STACK_CURSOR_SLOT_FACTS = 20,
} menu_stack_cursor_slot_e;

enum { MENU_STACK_CURSOR_SLOT_COUNT = 21 };

extern menu_cursor_state_t g_menu_stack_cursor_states[];

void menu_stack_clamp_cursor(s32 index, s32 count, s32 visible);
void menu_stack_clear_cursor(s32 index);
void menu_stack_store_cursor(s32 index);

/* list panels */
/* Per-row flag column of the list panels; the list steps compare it signed, and the two writers that only store
 * into it agree on the width. */
extern s16 g_menu_stack_row_flags[];

/* Row value column of the list panels, read signed as `value & 0x7ff`. */
extern s16 g_menu_stack_entry_values[];

#endif
