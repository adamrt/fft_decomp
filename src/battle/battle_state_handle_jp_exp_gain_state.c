#include "fft/battle.h"

/* Finish the JP/EXP gain state once the camera and map rotation settle.
 *
 * Event actions (type 0x200) hand off to the event system; otherwise a unit
 * whose turn continues reloads its last command into the misc command state
 * and resumes the pending attack phase or the wait-direction prompt. */
void battle_state_handle_jp_exp_gain_state(void) {
    battle_unit_misc_data_t* unit;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    unit = battle_unit_get_source_misc_data();
    if ((g_battle_current_vector.vx | g_battle_current_vector.vy | g_battle_current_vector.vz) != 0) {
        return;
    }
    if (g_battle_camera_rotation_action != 0) {
        return;
    }
    if (g_action_type == BATTLE_TURN_EVENT_ABILITY_READY) {
        if (battle_menu_init_system_function(8, 2, unit->battle_data->misc_unit_id, 0, 1) == 2
            && battle_script_get_event_finish_operation() != 0) {
            g_previous_battle_game_state = g_battle_game_state;
            battle_menu_init_system_function(8, 0, unit->battle_data->misc_unit_id, 0, 1);
            battle_action_set_casting_unit_id_ff();
            return;
        }
        battle_action_check_between_turn_events();
        return;
    }
    if (battle_action_check_change_of_turn(unit->battle_data->misc_unit_id) == 1) {
        /* battle_stats_t +0x16e holds the same 20-byte command payload. */
        unit->command_state.ai.data.action = *(battle_ai_command_action_t*)&unit->battle_data->action_actor_id;
        unit->used_ability_id = unit->battle_data->last_ability_id;
        if (unit->attack_phase_state == 3) {
            unit->attack_phase_state = 0;
            battle_gfx_reset_unit_graphic_trigger(unit->unit_id);
            battle_action_check_between_turn_events();
            return;
        }
        battle_action_choose_wait();
        return;
    }
    battle_menu_set_next_script_action_menus();
}
