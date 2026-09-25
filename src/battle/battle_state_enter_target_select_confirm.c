#include "fft/battle.h"

void battle_state_enter_target_select_confirm(void) {
    battle_unit_misc_data_t* misc;
    battle_stats_t* stats;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_CONFIRM;
    misc = battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (misc != 0) {
        stats = misc->battle_data;
        if (stats != 0) {
            battle_menu_store_unit_names_and_event_block_data(3, stats->misc_unit_id, 0);
        }
    }
    stats = battle_unit_get_casting_misc_data()->battle_data;
    battle_menu_init_system_function(9, 0, stats->misc_unit_id, 0, 1);
}
