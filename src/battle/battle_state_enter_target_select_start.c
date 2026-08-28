#include "fft/battle.h"

void battle_state_enter_target_select_start(void) {
    battle_unit_misc_data_t* unit;
    battle_stats_t* stats;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_START;
    unit = battle_unit_get_casting_misc_data();
    stats = unit->battle_data;
    battle_menu_init_system_function(1, 0, stats->misc_unit_id, 0, 1);
    battle_action_set_at_list_active();
}
