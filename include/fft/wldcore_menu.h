#ifndef WLDCORE_MENU_H
#define WLDCORE_MENU_H

/* WLDCORE's menu levels and lists: the bar, Brave Story, tutorials, treasures and unexplored lands. */

#include "fft/menu.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* Phases of a list-window level (wldcore_menu_list_window_level_t.mode). The
 * town menu uses only the first two. */
typedef enum wldcore_list_window_mode {
    WLDCORE_LIST_WINDOW_MODE_BROWSE = 0,
    WLDCORE_LIST_WINDOW_MODE_LEAVE_MESSAGE = 1, /* the farewell text is up; waits for circle or cross */
    WLDCORE_LIST_WINDOW_MODE_FADE_OR_HELP = 2,  /* fading out after the farewell, or under the help message */
    WLDCORE_LIST_WINDOW_MODE_LEAVE_WAIT = 3,    /* faded out; waits for thread 14 to end, then pops */
} wldcore_list_window_mode_e;

/* Row kinds of the town menu list (wldcore_bar_build_menu_rows); the row text
 * is 0xb800 + kind. */
typedef enum wldcore_bar_menu_row {
    WLDCORE_BAR_MENU_ROW_RUMORS = 0,
    WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS = 1,
    WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS = 2,
    WLDCORE_BAR_MENU_ROW_PROPOSITIONS = 3,
    WLDCORE_BAR_MENU_ROW_LEAVE = 4,
} wldcore_bar_menu_row_e;

/* Row kinds of the brave-story list (wldcore_menu_build_brave_story_entries),
 * named after the level each row opens. */
typedef enum wldcore_brave_story_row {
    WLDCORE_BRAVE_STORY_ROW_SCRIPT_FLAGS_01A4_01BB = 0, /* the facts (EVENT_SCRIPT_VAR_FACT_FIRST) */
    WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS = 1,
    WLDCORE_BRAVE_STORY_ROW_SET_SCRIPT_VARIABLES_03C0_03FF = 2, /* the persons (EVENT_SCRIPT_VAR_PERSON_PROFILE_*) */
    WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS = 3,
    WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS = 4,
    WLDCORE_BRAVE_STORY_ROW_TREASURES = 5,
    WLDCORE_BRAVE_STORY_ROW_LEAVE = 6,
} wldcore_brave_story_row_e;

/* Row kinds of the tutorial category list (wldcore_list_open_tutorial_categories). */
typedef enum wldcore_tutorial_category_row {
    WLDCORE_TUTORIAL_CATEGORY_ROW_FORMATION = 0,
    WLDCORE_TUTORIAL_CATEGORY_ROW_MASK_1_ENTRIES = 1,
    WLDCORE_TUTORIAL_CATEGORY_ROW_TOPICS = 2,
    WLDCORE_TUTORIAL_CATEGORY_ROW_LEAVE = 3,
} wldcore_tutorial_category_row_e;

typedef char
    wldcore_menu_location_transition_level_size_must_be_92[sizeof(wldcore_menu_location_transition_level_t) == 92 ? 1
                                                                                                                  : -1];

/* Menu-stack handler selectors. Push routines supply the level record;
 * RETURN bridges the entry window and a pending world-map transition. */
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
    WLDCORE_MENU_LEVEL_SET_SCRIPT_VARIABLES_03C0_03FF = 31,
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
    WLDCORE_MENU_LEVEL_SCRIPT_FLAGS_01A4_01BB = 46,
    WLDCORE_MENU_LEVEL_SCRIPT_FLAG_01A4_DETAIL = 47,
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

/* story */
extern menu_cursor_state_t g_wldcore_story_events_cursor_state;
extern u8 (*g_wldcore_brave_story_birthdays)[2];
extern u8 g_wldcore_brave_story_help_text_ids[];
extern s32 g_wldcore_brave_story_saved_cursor[];
extern u16* g_wldcore_story_event_ids;
/* Provisional: milestone threshold tables at 0x8009eb34 (eight entries,
 * 1/4/8/12/16/20/24/31) and 0x8009eb3c (1/3/6/8/10/12/14/16). */
extern u8 g_wldcore_treasure_count_milestones[];
extern u8 g_wldcore_treasure_picture_sets[];
extern u8 g_wldcore_treasure_sound_novel_ids[];
extern u8 g_wldcore_unexplored_land_count_milestones[];
extern u8 g_wldcore_unexplored_land_picture_sets[];

/* bar */
extern s32 g_wldcore_bar_menu_saved_cursor;
extern u8 g_wldcore_bar_row_help_text_ids[];
void wldcore_bar_active_propositions_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_bar_build_menu_rows(wldcore_menu_list_window_level_t* level);

/* list */
extern menu_cursor_state_t g_wldcore_tutorial_entries_cursor_state;
extern s32 g_wldcore_tutorial_category_saved_cursor;
extern s32 g_wldcore_tutorial_event_ids[];
/* Per-row flag column of the world list panels; the list steps compare it
 * signed, and the two writers that only store into it agree on the width. */
extern s16 g_wldcore_list_row_flags[];
/* Row value column of the world list panels, read signed as `value & 0x7ff`. */
extern s16 g_wldcore_list_entry_values[];
void wldcore_list_open_script_flags_01a4_01bb(void);
void wldcore_list_clamp_cursor_state(s32 index, s32 count, s32 visible);
void wldcore_list_clear_cursor_state(s32 index);
void wldcore_list_open_tutorial_topics(void);
void wldcore_list_store_cursor_state(s32 index);
/* The list-level steps hand their menu level to these stubs, which read no
 * argument; the parameter is the argument the target passes in $a0. */
void wldcore_list_rumors_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_propositions_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_unexplored_lands_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_story_events_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_script_variables_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_tutorial_topics_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_formation_tutorials_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_script_flags_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_open_available_propositions(void);
void wldcore_list_open_completed_propositions(void);
void wldcore_list_open_propositions(void);
void wldcore_list_tutorial_mask_1_block_all_buttons(wldcore_menu_panel_level_t* unused_level);
void wldcore_list_open_unexplored_lands(void);
void wldcore_list_open_treasures(void);

/* menu */
extern menu_cursor_state_t g_wldcore_menu_cursor_states[];
/* libgs sprite sorted by wldcore_draw_close_indicator_if_pending through
 * GsSortSprite (0x800e0480). */
extern GsSPRITE g_wldcore_menu_indicator_sprite;
extern s32 g_wldcore_menu_stack_depth;
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records[];
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records_next[];
extern s32 g_wldcore_menu_stack_types[];
extern s32 g_wldcore_menu_stack_types_next[];
extern void (*g_wldcore_menu_idle_handlers[])(wldcore_menu_stack_record_t*);
extern s32 g_wldcore_menu_ordering_table_offset;
extern void (*g_wldcore_menu_publish_handlers[])(wldcore_menu_stack_record_t*);
extern void (*g_wldcore_menu_resume_handlers[])(wldcore_menu_stack_record_t*);
extern s32 g_wldcore_menu_root_window_index;
void wldcore_menu_push_location_menu_level(s32 menu);
void wldcore_menu_push_event_transition_level(s32 mode);
void wldcore_menu_dispatch_idle_handler(void);
void wldcore_menu_dispatch_pending_script_requests(void);
void wldcore_menu_dispatch_publish_handler(void);
void wldcore_menu_dispatch_resume_handler(void);
void wldcore_bar_handle_active_propositions_input(wldcore_menu_stack_record_t* level);
void wldcore_menu_handle_story_event_text_input(wldcore_menu_stack_record_t* level);
void wldcore_list_handle_completed_propositions_input(wldcore_menu_stack_record_t* level);
void wldcore_list_completed_propositions_block_all_buttons(wldcore_menu_stack_record_t* unused_level);
void wldcore_menu_resume_root_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_push_message_level(s32 text, s32 reload_text);
void wldcore_menu_reset_window_list_and_store_origin(void);
void wldcore_menu_run_world_frame(GsOT* context);
void wldcore_list_build_proposition_detail_panel_image(
    wldcore_menu_stack_record_t* level, s32 proposition, s32 coordinate_mode);
void wldcore_menu_handle_participant_list_input(wldcore_menu_stack_record_t* level);
void wldcore_menu_step_map_cursor_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_push_screen_transition_level(s32 screen, s32 param_a, s32 param_b);
void wldcore_menu_push_story_event_text_level(s32 value);
void wldcore_menu_push_treasure_detail_level(s32 value);
void wldcore_menu_push_entry_window_level(void);
void wldcore_menu_push_layout_window_level(void);
void wldcore_menu_push_brave_story_level(void);
void wldcore_menu_push_scrollable_text_window_level(s32 text_id);
void wldcore_menu_push_number_panel_level(void);
void wldcore_menu_push_text_window_level(s32 text, s32 reload_text);
void wldcore_menu_push_countdown_level(void);

#endif
