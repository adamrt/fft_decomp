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
extern s32 g_menu_stack_depth;
extern s32 g_menu_stack_types[]; /* menu_stack_level_e of each level */
extern menu_stack_record_t g_menu_stack_records[];
/* The record and type arrays offset by one level, which the push routines index by the current depth: spelling
 * that as g_menu_stack_records[depth + 1] changes their address arithmetic. The type view is kept only
 * where types[depth + 1] reschedules the push routine (proposition detail, tutorial topic text, the Brave Story and
 * the persons list). */
extern s32 g_menu_stack_types_next[];
extern menu_stack_record_t g_menu_stack_records_next[];
/* Handlers by level type. Idle runs the top level each frame; suspend runs just before another level opens over it
 * (list levels withhold every button, window levels dim their windows); resume undoes that when the level above
 * closes. */
extern void (*g_menu_stack_idle_handlers[])(menu_stack_record_t*);
extern void (*g_menu_stack_suspend_handlers[])(menu_stack_record_t*);
extern void (*g_menu_stack_resume_handlers[])(menu_stack_record_t*);
extern s32 g_menu_stack_ot_offset;
extern s32 g_world_map_cursor_window;
extern s16 g_window_panel_value;
/* The event transition's spinning screen-capture sprite, sorted with GsSortSprite by
 * event_transition_draw_sprite. */
extern GsSPRITE g_event_transition_sprite;
void menu_stack_reset(void);
void world_map_run_world_menus(GsOT* context);
void menu_stack_dispatch_idle(void);
void menu_stack_dispatch_suspend(void);
void menu_stack_dispatch_resume(void);
void location_script_dispatch_requests(void);
void world_map_resume_cursor_level(menu_stack_record_t* level);
void sound_novel_resume_level(menu_stack_record_t* level);
void world_map_step_cursor_level(menu_stack_record_t* level);
void event_transition_draw_sprite(GsOT* ot);

/* push routines */
void world_map_push_menu_level(void);
void unit_panel_push_help_level(void);
void menu_stack_push_message_level(s32 text, s32 reload_text);
void menu_stack_push_text_window_level(s32 text, s32 reload_text);
void menu_stack_push_scrollable_text_level(s32 text_id);
void proposition_push_days_level(void);
void location_push_menu_level(s32 menu);
void screen_transition_push_level(s32 screen, s32 param_a, s32 param_b);
void event_transition_push_level(s32 mode);
void proposition_push_discovery_level(void);

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
/* Withhold every button (g_menu_blocked_buttons = -1) before a list level opens a message over itself. The list
 * steps hand each its level, which it does not read: the parameter is only what the target passes in $a0. */
void rumor_block_list_buttons(menu_stack_panel_level_t* level);
void proposition_block_available_list_buttons(menu_stack_panel_level_t* level);
void brave_story_block_unexplored_land_list_buttons(menu_stack_panel_level_t* level);
void brave_story_block_event_list_buttons(menu_stack_panel_level_t* level);
void brave_story_block_person_list_buttons(menu_stack_panel_level_t* level);
void tutorial_block_topic_list_buttons(menu_stack_panel_level_t* level);
void tutorial_block_formation_list_buttons(menu_stack_panel_level_t* level);
void brave_story_block_fact_list_buttons(menu_stack_panel_level_t* level);
void tutorial_block_battle_list_buttons(menu_stack_panel_level_t* unused_level);
void proposition_block_returned_list_buttons(menu_stack_record_t* unused_level);
void proposition_block_active_list_buttons(menu_stack_panel_level_t* level);

/* bar */
/* Row kinds of the bar's menu (bar_build_rows); the row text is TEXT_ID_WORLD_MENU_BASE + kind. */
typedef enum bar_row {
    BAR_ROW_RUMORS = 0,
    BAR_ROW_AVAILABLE_PROPOSITIONS = 1,
    BAR_ROW_RETURNED = 2,
    BAR_ROW_ACTIVE_PROPOSITIONS = 3,
    BAR_ROW_LEAVE = 4,
} bar_row_e;

extern s32 g_bar_saved_cursor;
extern u8 g_bar_row_help_text_ids[];
extern s32* g_rumor_location_masks;
void bar_build_rows(menu_stack_window_level_t* level);
void bar_resume_level(menu_stack_record_t* level);
void proposition_handle_active_list_input(menu_stack_record_t* level);
void proposition_open_available_list(void);
void proposition_open_returned_list(void);
void proposition_handle_returned_list_input(menu_stack_record_t* level);
void proposition_open_active_list(void);

/* Brave Story */
/* Row kinds of the Brave Story list (brave_story_build_rows), named after the level each row
 * opens. */
typedef enum brave_story_row {
    BRAVE_STORY_ROW_FACTS = 0, /* EVENT_SCRIPT_VAR_FACT_FIRST */
    BRAVE_STORY_ROW_EVENTS = 1,
    BRAVE_STORY_ROW_PERSONS = 2, /* EVENT_SCRIPT_VAR_PERSON_PROFILE_FIRST */
    BRAVE_STORY_ROW_PROPOSITIONS = 3,
    BRAVE_STORY_ROW_UNEXPLORED_LANDS = 4,
    BRAVE_STORY_ROW_TREASURES = 5,
    BRAVE_STORY_ROW_LEAVE = 6,
} brave_story_row_e;

extern s32 g_brave_story_saved_cursor[];
extern u8 g_brave_story_help_text_ids[];
extern u8 (*g_brave_story_birthdays)[2];
extern menu_cursor_state_t g_brave_story_event_list_cursor;
extern u16* g_brave_story_event_ids;
/* The collection counts that earn a milestone message (proposition_get_milestone_rank): eight each,
 * 1/4/8/12/16/20/24/31 treasures and 1/3/6/8/10/12/14/16 unexplored lands. */
extern u8 g_proposition_treasure_milestones[];
extern u8 g_proposition_unexplored_land_milestones[];
extern u8 g_treasure_picture_sets[];
extern u8 g_unexplored_land_picture_sets[];
extern u8 g_treasure_sound_novel_ids[];
void brave_story_push_level(void);
void brave_story_push_event_level(s32 value);
void brave_story_handle_event_input(menu_stack_record_t* level);
void brave_story_push_treasure_level(s32 value);
void brave_story_open_fact_list(void);
void brave_story_open_unexplored_land_list(void);
void brave_story_open_treasure_list(void);

/* tutorials */
/* Row kinds of the tutorial category list (tutorial_push_menu_level). */
typedef enum tutorial_menu_row {
    TUTORIAL_MENU_ROW_FORMATION = 0,
    TUTORIAL_MENU_ROW_BATTLE = 1,
    TUTORIAL_MENU_ROW_TOPICS = 2,
    TUTORIAL_MENU_ROW_LEAVE = 3,
} tutorial_menu_row_e;

extern s32 g_tutorial_menu_saved_cursor;
extern menu_cursor_state_t g_tutorial_list_cursor;
extern s32 g_tutorial_battle_event_ids[];
void tutorial_open_topic_list(void);
void tutorial_resume_menu_level(menu_stack_record_t* level);

#endif
