#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_target.h"

/*
 * Enter action-execution setup after panel preparation.
 *
 * Panel result 3 selects the alternate setup argument. AT-list state
 * follows the unit's control flag after the setup call returns.
 */
void battle_state_enter_action_execution_setup(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_ACTION_EXECUTE_SETUP;
    unit = battle_unit_get_source();
    if (unit->ability_preview_phase == TARGET_PANELS_NONE)
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 1, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    else
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, unit->stats->battle_id, 0,
            unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
    if (unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        action_set_at_list_active();
    else
        action_clear_at_list_active();
}
