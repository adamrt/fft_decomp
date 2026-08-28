#include "fft/battle.h"

void battle_action_enter_move_range_exception(void) {
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_MOVE_RANGE_EXCEPTION;
    unit = battle_unit_get_source_misc_data();
    battle_menu_init_system_function(
        1, 2, unit->battle_data->misc_unit_id, 0, unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}
