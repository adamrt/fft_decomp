#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Enter the target-select-start state: open the targeting prompt for the casting
 * unit. */
void battle_state_enter_target_select_start(void) {
    battle_unit_t* unit;
    unit_stats_t* stats;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_START;
    unit = battle_unit_get_casting();
    stats = unit->stats;
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_TARGETING_PROMPT, 0, stats->battle_id, 0, 1);
    action_set_at_list_active();
}
