#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_script.h"

/* JpExpGain game-state handler: move on once the camera has settled (no cursor
 * vector, no rotation).
 *
 * After a charged ability (turn event ABILITY_READY) a queued scenario event with
 * a nonzero finish operation starts; otherwise the between-turn events run. On a
 * unit's own turn, a unit whose turn is over (action_check_change_of_turn
 * returns 1) gets its last command and ability copied back into its runtime record;
 * a Jump then resets its preparation and goes to the between-turn events, and
 * anything else opens the Wait menu. A unit whose turn goes on gets its action
 * menus back. */
void battle_state_handle_jp_exp_gain_state(void) {
    battle_unit_t* unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_main_input_current_buttons);
    unit = battle_unit_get_source();
    if ((g_camera_step_vector.vx | g_camera_step_vector.vy | g_camera_step_vector.vz) != 0) {
        return;
    }
    if (g_camera_rotation_action != CAMERA_ROTATION_NONE) {
        return;
    }
    if (g_action_turn_event_type == BATTLE_TURN_EVENT_ABILITY_READY) {
        if (menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 2, unit->stats->battle_id, 0, 1) == 2
            && script_get_event_finish_operation() != 0) {
            g_battle_previous_game_state = g_battle_game_state;
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_SCENARIO_CHECK, 0, unit->stats->battle_id, 0, 1);
            action_set_casting_unit_id_ff();
            return;
        }
        action_check_between_turn_events();
        return;
    }
    if (action_check_change_of_turn(unit->stats->battle_id) == 1) {
        /* unit_stats_t +0x16e holds the same 20-byte command payload. */
        unit->command_state.ai.data.action = unit->stats->action_command;
        unit->used_ability_id = unit->stats->action_command.ability_id;
        if (unit->attack_phase_state == BATTLE_ATTACK_PREPARATION_JUMP) {
            unit->attack_phase_state = BATTLE_ATTACK_PREPARATION_CHARGE;
            battle_gfx_reset_unit_graphic_trigger(unit->runtime_id);
            action_check_between_turn_events();
            return;
        }
        action_choose_wait();
        return;
    }
    battle_menu_set_next_script_action_menus();
}
