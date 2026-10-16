#ifndef WLDCORE_MENU_H
#define WLDCORE_MENU_H

/* WLDCORE's menu stack and the lists built on it: the stack with its handler tables and push routines, remembered
 * cursors, and the bar, Brave Story and tutorial lists. The level records are in wldcore_menu_level.h. */

#include "fft/menu.h"
#include "fft/menu_selection.h"
#include "fft/wldcore_menu_level.h"
#include "psx/libgs.h"
#include "psx/types.h"

/* menu stack */
extern s32 g_wldcore_menu_stack_depth;
extern s32 g_wldcore_menu_stack_types[]; /* wldcore_menu_level_type_e of each level */
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records[];
/* The record and type arrays offset by one level, which the push routines index by the current depth: spelling
 * that as g_wldcore_menu_stack_records[depth + 1] changes their address arithmetic. The type view is kept only
 * where types[depth + 1] reschedules the push routine (proposition detail, tutorial topic text, the Brave Story and
 * the 0x03c0-0x03ff variable lists). */
extern s32 g_wldcore_menu_stack_types_next[];
extern wldcore_menu_stack_record_t g_wldcore_menu_stack_records_next[];
/* Handlers by level type. Idle runs the top level each frame; publish runs just before another level opens over it
 * (list levels withhold every button, window levels dim their windows); resume undoes that when the level above
 * closes. */
extern void (*g_wldcore_menu_idle_handlers[])(wldcore_menu_stack_record_t*);
extern void (*g_wldcore_menu_publish_handlers[])(wldcore_menu_stack_record_t*);
extern void (*g_wldcore_menu_resume_handlers[])(wldcore_menu_stack_record_t*);
extern s32 g_wldcore_menu_ordering_table_offset;
extern s32 g_wldcore_menu_root_window_index;
extern s16 g_wldcore_active_menu_value;
/* The close indicator, sorted with GsSortSprite by wldcore_draw_close_indicator_if_pending. */
extern GsSPRITE g_wldcore_menu_indicator_sprite;
void wldcore_menu_reset_stack(void);
void wldcore_menu_run_world_frame(GsOT* context);
void wldcore_menu_dispatch_idle_handler(void);
void wldcore_menu_dispatch_publish_handler(void);
void wldcore_menu_dispatch_resume_handler(void);
void wldcore_menu_dispatch_pending_script_requests(void);
void wldcore_menu_resume_root_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_resume_sound_novel_level(wldcore_menu_stack_record_t* level);
void wldcore_menu_step_map_cursor_level(wldcore_menu_stack_record_t* level);
void wldcore_draw_close_indicator_if_pending(GsOT* ot);

/* push routines */
void wldcore_menu_push_entry_window_level(void);
void wldcore_menu_push_layout_window_level(void);
void wldcore_menu_push_message_level(s32 text, s32 reload_text);
void wldcore_menu_push_text_window_level(s32 text, s32 reload_text);
void wldcore_menu_push_scrollable_text_window_level(s32 text_id);
void wldcore_menu_push_number_panel_level(void);
void wldcore_menu_push_location_menu_level(s32 menu);
void wldcore_menu_push_screen_transition_level(s32 screen, s32 param_a, s32 param_b);
void wldcore_menu_push_event_transition_level(s32 mode);
void wldcore_menu_push_countdown_level(void);

/* remembered cursors */
/* Slots of g_wldcore_menu_cursor_states: the remembered row of each list. */
typedef enum wldcore_cursor_slot {
    WLDCORE_CURSOR_SLOT_LOCATION_MENU = 0,
    WLDCORE_CURSOR_SLOT_BAR_MENU = 1,
    WLDCORE_CURSOR_SLOT_RUMORS = 2,
    WLDCORE_CURSOR_SLOT_AVAILABLE_PROPOSITIONS = 3,
    WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS = 4,
    WLDCORE_CURSOR_SLOT_PROPOSITIONS = 5,
    WLDCORE_CURSOR_SLOT_UNEXPLORED_LANDS = 7,
    WLDCORE_CURSOR_SLOT_TREASURES = 8,
    WLDCORE_CURSOR_SLOT_STORY_EVENTS = 10,
    WLDCORE_CURSOR_SLOT_PERSONS = 11,
    WLDCORE_CURSOR_SLOT_TUTORIAL_TOPICS = 12,
    WLDCORE_CURSOR_SLOT_PARTICIPANT_LIST = 15,
    WLDCORE_CURSOR_SLOT_BRAVE_STORY_PROPOSITIONS = 16,
    WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS = 18,
    WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES = 19,
    WLDCORE_CURSOR_SLOT_FACTS = 20,
} wldcore_cursor_slot_e;

enum { WLDCORE_CURSOR_SLOT_COUNT = 21 };

extern menu_cursor_state_t g_wldcore_menu_cursor_states[];
void wldcore_list_clamp_cursor_state(s32 index, s32 count, s32 visible);
void wldcore_list_clear_cursor_state(s32 index);
void wldcore_list_store_cursor_state(s32 index);

/* list panels */
/* Per-row flag column of the list panels; the list steps compare it signed, and the two writers that only store
 * into it agree on the width. */
extern s16 g_wldcore_list_row_flags[];
/* Row value column of the list panels, read signed as `value & 0x7ff`. */
extern s16 g_wldcore_list_entry_values[];
/* Withhold every button (g_menu_blocked_buttons = -1) before a list level opens a message over itself. The list
 * steps hand each its level, which it does not read: the parameter is only what the target passes in $a0. */
void wldcore_list_rumors_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_propositions_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_unexplored_lands_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_story_events_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_persons_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_tutorial_topics_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_formation_tutorials_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_facts_block_all_buttons(wldcore_menu_panel_level_t* level);
void wldcore_list_battle_tutorials_block_all_buttons(wldcore_menu_panel_level_t* unused_level);
void wldcore_list_completed_propositions_block_all_buttons(wldcore_menu_stack_record_t* unused_level);
void wldcore_bar_active_propositions_block_all_buttons(wldcore_menu_panel_level_t* level);

/* bar */
/* Row kinds of the bar's menu (wldcore_bar_build_menu_rows); the row text is TEXT_ID_WORLD_MENU_BASE + kind. */
typedef enum wldcore_bar_menu_row {
    WLDCORE_BAR_MENU_ROW_RUMORS = 0,
    WLDCORE_BAR_MENU_ROW_AVAILABLE_PROPOSITIONS = 1,
    WLDCORE_BAR_MENU_ROW_COMPLETED_PROPOSITIONS = 2,
    WLDCORE_BAR_MENU_ROW_PROPOSITIONS = 3,
    WLDCORE_BAR_MENU_ROW_LEAVE = 4,
} wldcore_bar_menu_row_e;

extern s32 g_wldcore_bar_menu_saved_cursor;
extern u8 g_wldcore_bar_row_help_text_ids[];
extern s32* g_wldcore_rumor_location_masks;
void wldcore_bar_build_menu_rows(wldcore_menu_list_window_level_t* level);
void wldcore_bar_restore_list_and_show_welcome(wldcore_menu_stack_record_t* level);
void wldcore_bar_handle_active_propositions_input(wldcore_menu_stack_record_t* level);
void wldcore_list_open_available_propositions(void);
void wldcore_list_open_completed_propositions(void);
void wldcore_list_handle_completed_propositions_input(wldcore_menu_stack_record_t* level);
void wldcore_list_open_propositions(void);

/* Brave Story */
/* Row kinds of the Brave Story list (wldcore_menu_build_brave_story_entries), named after the level each row
 * opens. */
typedef enum wldcore_brave_story_row {
    WLDCORE_BRAVE_STORY_ROW_FACTS = 0, /* EVENT_SCRIPT_VAR_FACT_FIRST */
    WLDCORE_BRAVE_STORY_ROW_STORY_EVENTS = 1,
    WLDCORE_BRAVE_STORY_ROW_PERSONS = 2, /* EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST */
    WLDCORE_BRAVE_STORY_ROW_PROPOSITIONS = 3,
    WLDCORE_BRAVE_STORY_ROW_UNEXPLORED_LANDS = 4,
    WLDCORE_BRAVE_STORY_ROW_TREASURES = 5,
    WLDCORE_BRAVE_STORY_ROW_LEAVE = 6,
} wldcore_brave_story_row_e;

extern s32 g_wldcore_brave_story_saved_cursor[];
extern u8 g_wldcore_brave_story_help_text_ids[];
extern u8 (*g_wldcore_brave_story_birthdays)[2];
extern menu_cursor_state_t g_wldcore_story_events_cursor_state;
extern u16* g_wldcore_story_event_ids;
/* The collection counts that earn a milestone message (wldcore_get_completion_milestone_rank): eight each,
 * 1/4/8/12/16/20/24/31 treasures and 1/3/6/8/10/12/14/16 unexplored lands. */
extern u8 g_wldcore_treasure_count_milestones[];
extern u8 g_wldcore_unexplored_land_count_milestones[];
extern u8 g_wldcore_treasure_picture_sets[];
extern u8 g_wldcore_unexplored_land_picture_sets[];
extern u8 g_wldcore_treasure_sound_novel_ids[];
void wldcore_menu_push_brave_story_level(void);
void wldcore_menu_push_story_event_text_level(s32 value);
void wldcore_menu_handle_story_event_text_input(wldcore_menu_stack_record_t* level);
void wldcore_menu_push_treasure_detail_level(s32 value);
void wldcore_list_open_facts(void);
void wldcore_list_open_unexplored_lands(void);
void wldcore_list_open_treasures(void);

/* tutorials */
/* Row kinds of the tutorial category list (wldcore_list_open_tutorial_categories). */
typedef enum wldcore_tutorial_category_row {
    WLDCORE_TUTORIAL_CATEGORY_ROW_FORMATION = 0,
    WLDCORE_TUTORIAL_CATEGORY_ROW_MASK_1_ENTRIES = 1,
    WLDCORE_TUTORIAL_CATEGORY_ROW_TOPICS = 2,
    WLDCORE_TUTORIAL_CATEGORY_ROW_LEAVE = 3,
} wldcore_tutorial_category_row_e;

extern s32 g_wldcore_tutorial_category_saved_cursor;
extern menu_cursor_state_t g_wldcore_tutorial_entries_cursor_state;
extern s32 g_wldcore_tutorial_event_ids[];
void wldcore_list_open_tutorial_topics(void);
void wldcore_tutorial_restore_list_and_show_welcome(wldcore_menu_stack_record_t* level);

#endif
