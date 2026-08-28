#ifndef FFT_BATTLE_STATE_H
#define FFT_BATTLE_STATE_H

#include "psx/types.h"

extern s32 g_battle_controller_input;
extern s32 g_battle_game_state;
extern s32 g_battle_menu_status_enabled;
extern s32 g_battle_state_animation_continue_check;
extern s32 g_battle_state_game_flow_running;

void battle_state_enter_action_execution_setup(void);
void battle_state_enter_after_command(void);
void battle_state_enter_highlight_units_by_team(void);
void battle_state_enter_jp_xp_gain(void);
void battle_state_enter_status_execute(void);
void battle_state_enter_target_display(void);
void battle_state_enter_target_out_of_range(void);
void battle_state_enter_target_select(void);
void battle_state_enter_target_select_confirm(void);
void battle_state_enter_target_select_denied(void);
void battle_state_enter_unit_moving(void);
void battle_state_stop_map_animations(void);
s32 battle_state_get_animation_speed(void);
void battle_state_handle_ability_preview_help_state(void);
void battle_state_handle_ability_preview_state(void);
void battle_state_handle_action_cast_state(void);
void battle_state_handle_action_execute_setup_state(void);
void battle_state_update_action_execute_mode(void);
void battle_state_handle_action_help_menu_state(void);
void battle_state_handle_active_turn_state(void);
void battle_state_handle_after_command_state(void);
void battle_state_handle_battle_message_display_state(void);
void battle_state_handle_change_turn_state(void);
void battle_state_handle_close_battle_state(void);
void battle_state_handle_close_move_help_state(void);
void battle_state_handle_commence_attack_phase_state(void);
void battle_state_handle_confirm_action_state(void);
void battle_state_handle_crystal_learn_state(void);
void battle_state_handle_deep_dungeon_mesh_finish_state(void);
void battle_state_handle_deep_dungeon_mesh_load_state(void);

/* Per-frame handlers dispatched by battle_state_run_game_loop. */
void battle_state_handle_default_state(void);
void battle_state_handle_display_move_area_state(void);
void battle_state_handle_effect_damage_display_state(void);
void battle_state_handle_effect_state(void);
void battle_state_update_event_mode(void);
void battle_state_handle_free_cursor_help_state(void);
void battle_state_handle_free_cursor_input(void);
void battle_state_handle_highlight_units_state(void);
void battle_state_handle_idling_action_menus_state(void);
void battle_state_handle_illegal_move_menu_state(void);
void battle_state_handle_illegal_range_state(void);
void battle_state_handle_jp_exp_gain_state(void);
void battle_state_handle_learn_ability_on_hit_state(void);
void battle_state_handle_change_map_init_state(void);
void battle_state_handle_map_init_state(void);
void battle_state_handle_change_map_jumping_in_state(void);
void battle_state_handle_map_jumping_in_state(void);
void battle_state_handle_change_map_jumping_out_state(void);
void battle_state_handle_map_jumping_out_state(void);
void battle_state_handle_menu_to_targeting_state(void);
void battle_state_handle_mini_menu_help_state(void);
void battle_state_handle_mini_menu_state(void);
void battle_state_handle_move_confirm_menu_state(void);
void battle_state_handle_move_range_exception_state(void);
void battle_state_handle_open_action_menus_state(void);
void battle_state_handle_open_sp2_files_state(void);
void battle_state_handle_pre_attack_animation_state(void);
void battle_state_handle_resume_attack_phase_state(void);
void battle_state_handle_secondary_effect_state(void);
void battle_state_handle_start_effect_file_open_state(void);
void battle_state_handle_status_execute_state(void);
void battle_state_handle_target_display_start_state(void);
void battle_state_handle_target_display_state(void);
void battle_state_handle_target_select_confirm_state(void);
void battle_state_handle_target_select_denied_state(void);
void battle_state_handle_target_select_start_state(void);
void battle_state_handle_target_select_state(void);
void battle_state_handle_targeting_range_state(void);
void battle_state_handle_unit_move_state(void);
void battle_state_handle_unit_moving_setup_state(void);
void battle_state_handle_unit_moving_state(void);
void battle_state_handle_wait_direction_state(void);
void battle_state_handle_wait_menu_state(void);
void battle_state_restart_menu_to_targeting(void);
void battle_state_run_game_loop(void);
void battle_state_start_close_battle(s32 duration);
void battle_state_start_game_flow(void);
void battle_state_stop_game_flow(void);
s32 battle_state_sync_frame(u32 ordering_table);
s32 battle_state_update_controller_input(void);

s32 battle_state_announce_next_charged_action(void);
void battle_state_enter_target_display_start(void);
void battle_state_enter_target_select_start(void);
s32 battle_state_get_animation_continue_check(void);
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state);
void battle_state_init_deployment_display(s32 width, s32 height, s32 projection, u8 red, u8 green, u8 blue);
s32 battle_state_set_animation_speed(s32 speed);
void battle_state_set_free_cursor(void);
void battle_state_set_time_scale(s32 value);
void battle_state_start_battle_message_display(void);
void battle_state_start_change_map_jump_in(s32 duration);
void battle_state_start_map_jump_out(s32 map_id, s32 duration);
void battle_state_start_change_map_jump_out(s32 map_id, s32 duration);
s32 battle_state_sync_and_submit_deployment_frame(u32 ordering_table);
s32 battle_state_update_deployment_controller_input(void);
extern s32 g_battle_state_map_transition_step;

void battle_state_enter_unit_moving_setup(void);

#endif
