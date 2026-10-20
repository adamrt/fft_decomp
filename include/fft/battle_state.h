#ifndef BATTLE_STATE_H
#define BATTLE_STATE_H

/* BATTLE's game state machine: the frame loop, the handler and entry function of each game state, map jumps,
 * battle outcomes and the battle's close. */

#include "fft/system.h"
#include "fft/unit.h"
#include "psx/types.h"

/* game state */
/*
 * Values dispatched by battle_entrypoint, verified against the
 * target's switch table.  The target has no explicit handler for value 50.
 */
typedef enum battle_state {
    BATTLE_STATE_FREE_CURSOR = 0,
    BATTLE_STATE_FREE_CURSOR_HELP = 1,
    BATTLE_STATE_HIGHLIGHT_UNITS = 2,
    BATTLE_STATE_OPEN_ACTION_MENUS = 3,
    BATTLE_STATE_IDLING_ACTION_MENUS = 4,
    BATTLE_STATE_AI_COMMAND = 5,
    BATTLE_STATE_ACTION_HELP_MENU = 6,
    BATTLE_STATE_DISPLAY_MOVE_AREA = 7,
    BATTLE_STATE_MINI_MENU = 8,
    BATTLE_STATE_MINI_MENU_HELP = 9,
    BATTLE_STATE_AT_LIST_INSPECTION = 10,
    BATTLE_STATE_STATUS_EXECUTE = 11,
    BATTLE_STATE_UNIT_MOVE = 12,
    BATTLE_STATE_SELECT_MOVE_TILE = 13,
    BATTLE_STATE_MOVE_RANGE_EXCEPTION = 14,
    BATTLE_STATE_ILLEGAL_MOVE_MENU = 15,
    BATTLE_STATE_UNIT_MOVING_SETUP = 16,
    BATTLE_STATE_UNIT_MOVING = 17,
    BATTLE_STATE_MOVE_CONFIRM_MENU = 18,
    BATTLE_STATE_WAIT_DIRECTION = 19,
    BATTLE_STATE_WAIT_MENU = 20,
    BATTLE_STATE_POST_MOVE_EVENTS = 21,
    BATTLE_STATE_TARGETING_PROMPT = 22,
    BATTLE_STATE_TARGETING_RANGE = 23,
    BATTLE_STATE_ILLEGAL_RANGE = 24,
    BATTLE_STATE_ABILITY_PREVIEW_HANDLING = 25,
    BATTLE_STATE_ABILITY_PREVIEW_HELP = 26,
    BATTLE_STATE_CONFIRM_ACTION = 27,
    BATTLE_STATE_PRE_ATTACK_ANIMATION = 28,
    BATTLE_STATE_ACTION_CAST = 29,
    BATTLE_STATE_COMMENCE_ATTACK_PHASE = 30,
    BATTLE_STATE_TARGET_SELECT_START = 31,
    BATTLE_STATE_TARGET_SELECT = 32,
    BATTLE_STATE_TARGET_SELECT_DENIED = 33,
    BATTLE_STATE_TARGET_SELECT_CONFIRM = 34,
    BATTLE_STATE_TARGET_DISPLAY_START = 35,
    BATTLE_STATE_TARGET_DISPLAY = 36,
    BATTLE_STATE_AFTER_COMMAND = 37,
    BATTLE_STATE_CONTINUE_TURN = 38,
    BATTLE_STATE_CHANGE_TURN = 39,
    BATTLE_STATE_POST_ACTION_REPORTS = 40,
    BATTLE_STATE_ACTION_ANNOUNCEMENT = 41,
    BATTLE_STATE_OPEN_SP2_FILES = 42,
    BATTLE_STATE_START_EFFECT_FILE_OPEN = 43,
    BATTLE_STATE_ACTION_EXECUTE_START = 44,
    BATTLE_STATE_ACTION_EXECUTE = 45,
    BATTLE_STATE_BATTLE_MESSAGE_DISPLAY = 46,
    BATTLE_STATE_RESUME_ATTACK_PHASE = 47,
    BATTLE_STATE_DEEP_DUNGEON_MESH_LOAD = 48,
    BATTLE_STATE_DEEP_DUNGEON_MESH_FINISH = 49,
    BATTLE_STATE_EFFECT = 51,
    BATTLE_STATE_EVENT = 52,
    BATTLE_STATE_MAP_JUMPING_OUT = 53,
    BATTLE_STATE_MAP_INITIALIZE = 54,
    BATTLE_STATE_MAP_JUMPING_IN = 55,
    BATTLE_STATE_MAP_JUMPING_OUT_2 = 56,
    BATTLE_STATE_MAP_INITIALIZE_2 = 57,
    BATTLE_STATE_MAP_JUMPING_IN_2 = 58,
    BATTLE_STATE_CLOSE_BATTLE = 59
} battle_state_e;

extern s32 g_battle_state;
extern s32 g_battle_state_previous;
extern s32 g_battle_map_init_step;

/* Nonzero while the action's effect is pending: 1 while its file loads, 2 while it plays, with
 * the knockback targets' step phases ORed in. SEQ ops 0xd3 and 0xd6 and
 * script_effect_wait wait on it. */
extern s32 g_battle_animation_wait_state;

/* Run BATTLE's deployment and game-state loop until control returns to MAIN. */
void battle_entrypoint(void);
s32 battle_frame_submit(u32* ordering_table);
s32 battle_input_update(void);
void battle_scene_update(void);
s32 battle_animation_wait_state_get(void);

/* state handlers */
/* Per-frame handlers of battle_state_e, in state order. */
void battle_state_free_cursor_update(void);
void battle_state_free_cursor_help_update(void);
void battle_state_highlight_units_update(void);
void battle_state_open_action_menus_update(void);
void battle_state_idling_action_menus_update(void);
void battle_state_ai_command_update(void);
void battle_state_action_help_menu_update(void);
void battle_state_display_move_area_update(void);
void battle_state_mini_menu_update(void);
void battle_state_mini_menu_help_update(void);
void battle_state_at_list_inspection_update(void);
void battle_state_status_execute_update(void);
void battle_state_unit_move_update(void);
void battle_state_select_move_tile_update(void);
void battle_state_move_range_exception_update(void);
void battle_state_illegal_move_menu_update(void);
void battle_state_unit_moving_setup_update(void);
void battle_state_unit_moving_update(void);
void battle_state_move_confirm_menu_update(void);
void battle_state_wait_direction_update(void);
void battle_state_wait_menu_update(void);
void battle_state_post_move_events_update(void);
void battle_state_targeting_prompt_update(void);
void battle_state_targeting_range_update(void);
void battle_state_illegal_range_update(void);
void battle_state_ability_preview_update(void);
void battle_state_ability_preview_help_update(void);
void battle_state_confirm_action_update(void);
void battle_state_pre_attack_animation_update(void);
void battle_state_action_cast_update(void);
void battle_state_commence_attack_phase_update(void);
void battle_state_target_select_start_update(void);
void battle_state_target_select_update(void);
void battle_state_target_select_denied_update(void);
void battle_state_target_select_confirm_update(void);
void battle_state_target_display_start_update(void);
void battle_state_target_display_update(void);
void battle_state_after_command_update(void);
void battle_state_continue_turn_update(void);
void battle_state_change_turn_update(void);
void battle_state_post_action_reports_update(void);
void battle_state_action_announcement_update(void);
void battle_state_open_sp2_files_update(void);
void battle_state_start_effect_file_open_update(void);
void battle_state_action_execute_start_update(void);
void battle_state_action_execute_update(void);
void battle_state_battle_message_display_update(void);
void battle_state_resume_attack_phase_update(void);
void battle_state_deep_dungeon_mesh_load_update(void);
void battle_state_deep_dungeon_mesh_finish_update(void);
void battle_state_effect_update(void);
void battle_state_event_update(void);
void battle_state_map_jumping_out_update(void);
void battle_state_map_init_update(void);
void battle_state_map_jumping_in_update(void);
void battle_state_change_map_jumping_out_update(void);
void battle_state_change_map_init_update(void);
void battle_state_change_map_jumping_in_update(void);
void battle_state_close_battle_update(void);

/* entering a state */
extern s32 g_battle_window_closed;

/* Each sets up a state and switches g_battle_state to it. */
void battle_state_free_cursor_enter(void);
void battle_state_free_cursor_help_enter(void);
void battle_state_highlight_units_enter(void);
void battle_state_open_action_menus_enter(void);
void battle_state_action_help_menu_enter(void);
void battle_state_display_move_area_enter(void);
void battle_state_mini_menu_enter(void);
void battle_state_return_to_mini_menu(void);
void battle_state_mini_menu_help_enter(void);
void battle_state_status_execute_enter(void);
void battle_state_unit_move_enter(void);
void battle_state_select_move_tile_enter(void);
void battle_state_illegal_move_menu_enter(void);
void battle_state_unit_moving_setup_enter(void);
void battle_state_unit_moving_enter(void);
void battle_state_move_confirm_menu_enter(void);
void battle_state_wait_direction_enter(void);
void battle_state_wait_menu_enter(void);
void battle_state_targeting_prompt_enter(void);
void battle_state_illegal_range_enter(void);
void battle_state_confirm_action_enter(void);
void battle_state_pre_attack_animation_enter(void);
void battle_state_action_cast_enter(void);
void battle_state_commence_attack_phase_enter(void);
void battle_state_target_select_enter(void);
void battle_state_target_select_denied_enter(void);
void battle_state_target_select_confirm_enter(void);
void battle_state_target_display_enter(void);
void battle_state_after_command_enter(void);
void battle_state_continue_turn_enter(void);
void battle_state_post_action_reports_enter(void);
void battle_state_open_sp2_files_enter(void);
void battle_state_action_execute_enter(void);
void battle_state_battle_message_display_enter(void);
void battle_state_event_enter(void);
void battle_state_enter_event_at_battle_start(void);

/* map jumps and closing the battle */
void battle_state_start_map_jump_out(s32 map_id, s32 duration);
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration);
void battle_state_start_change_map_jump_in(s32 duration);
void battle_state_enter_close_battle_to_world(s32 duration);

/* Queues CLOSE_BATTLE; the callers always pass transition step 8. */
void battle_state_close_battle_enter(s32 transition_step, system_game_flow_e close_flow_state);

/* event outcomes and scenarios */

s32 event_check_battle_outcome(void);
s32 event_try_enter_scenario(unit_runtime_t* unit);

#endif
