#include "fft/battle.h"

/*
 * Enter action-execution setup after panel preparation.
 *
 * Panel result 3 selects the alternate setup argument. AT-list state
 * follows the unit's control flag after the setup call returns.
 */
void battle_state_enter_action_execution_setup(void) {
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP;
    unit = battle_unit_get_source_misc_data();
    if (unit->ability_preview_phase == 3)
        battle_menu_init_system_function(
            1, 1, unit->battle_data->misc_unit_id, 0, unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    else
        battle_menu_init_system_function(
            1, 0, unit->battle_data->misc_unit_id, 0, unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        battle_action_set_at_list_active();
    else
        battle_action_clear_at_list_active();
}
