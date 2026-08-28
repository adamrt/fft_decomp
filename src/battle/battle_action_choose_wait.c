#include "fft/battle.h"

void battle_action_choose_wait(void) {
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    unit = battle_unit_get_source_misc_data();
    if (battle_menu_init_system_function(8, 2, unit->battle_data->misc_unit_id, 0, 1) == 2
        && battle_script_get_event_finish_operation() != 0) {
        g_previous_battle_game_state = g_battle_game_state;
        battle_menu_init_system_function(8, 0, unit->battle_data->misc_unit_id, 0, 1);
        battle_action_set_casting_unit_id_ff();
        return;
    }
    if (battle_status_check_unit(unit->battle_data) & BATTLE_TURN_STATUS_BLOCKS_WAIT_MENU_MASK) {
        battle_action_check_between_turn_events();
    } else {
        g_battle_game_state = BATTLE_GAME_STATE_WAIT_MENU;
        battle_target_store_cursor_unit_name_and_data();
        battle_menu_init_system_function(
            3, 0, unit->battle_data->misc_unit_id, 0, unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    }
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        battle_action_set_at_list_active();
    } else {
        battle_action_clear_at_list_active();
    }
}
