#ifndef MENU_STACK_RECORDS_H
#define MENU_STACK_RECORDS_H

/* WLDCORE's menu stack records: one slot per level, selected by its level type. */

#include "fft/bar.h"
#include "fft/brave_story.h"
#include "fft/location.h"
#include "fft/menu_stack.h"
#include "fft/proposition.h"
#include "fft/sound_novel.h"
#include "fft/system.h"
#include "fft/tutorial.h"
#include "fft/unit_panel.h"
#include "fft/world_map.h"
#include "psx/types.h"

/* Provisional: one 0x5c-byte slot of the menu stack (g_menu_stack_records), one per level; the level's type in
 * g_menu_stack_types selects the view. The leading 32-bit value normally names a window; the WORLD_MAP_FOCUS level
 * stores a map-dot index instead. */
union menu_stack_record {
    s32 window_index;
    s32 work_history_entry;   /* MENU_STACK_LEVEL_BRAVE_STORY_WORK_HISTORY_ENTRY */
    s32 tutorial_topic_index; /* MENU_STACK_LEVEL_TUTORIAL_TOPIC */
    s32 focus_dot_index;      /* MENU_STACK_LEVEL_WORLD_MAP_FOCUS */
    s32 story_event_index;    /* MENU_STACK_LEVEL_BRAVE_STORY_EVENT */
    unit_panel_help_level_t layout_window;
    menu_stack_message_level_t message;
    world_map_menu_level_t return_level;
    menu_stack_panel_level_t panel;
    proposition_discovery_level_t discovery; /* pushed by proposition_push_discovery_level */
    rumor_detail_level_t rumor_detail;
    brave_story_menu_level_t brave_story;
    bar_menu_level_t bar;
    brave_story_propositions_level_t brave_story_propositions;
    tutorial_menu_level_t tutorial_menu;
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
    proposition_accept_confirm_level_t accept_confirm;
    proposition_dispatch_confirm_level_t dispatch_confirm;
    proposition_detail_level_t proposition_detail;
    proposition_report_confirm_level_t proposition_report_confirm;
    world_map_cursor_level_t map_cursor;
    world_map_visibility_level_t visibility;
    world_map_path_level_t path;
    location_menu_state_t location_menu;
    location_menu_level_t location_list;
    proposition_report_level_t proposition_report;
    sound_novel_level_t sound_novel;
    proposition_result_level_t proposition_result;
};
typedef char menu_stack_record_size_must_be_92[sizeof(menu_stack_record_t) == 92 ? 1 : -1];

extern menu_stack_record_t g_menu_stack_records[MENU_STACK_CAPACITY];

#endif
