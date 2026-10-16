#ifndef WLDCORE_MENU_H
#define WLDCORE_MENU_H

/* WLDCORE's menu levels and lists: the bar, Brave Story, tutorials, treasures and unexplored lands. */

#include "fft/menu.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/types.h"

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
