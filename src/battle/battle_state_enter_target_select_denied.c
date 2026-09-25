#include "fft/battle.h"

void battle_state_enter_target_select_denied(void) {
    battle_unit_misc_data_t* misc;
    battle_unit_misc_data_t* unit;
    battle_stats_t* stats;
    s32 kind;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_TARGET_SELECT_DENIED;
    misc = battle_unit_get_selectable_misc_data_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (misc != 0) {
        stats = misc->battle_data;
        if (stats != 0) {
            battle_menu_store_unit_names_and_event_block_data(3, stats->misc_unit_id, 0);
        }
    }
    unit = battle_unit_get_casting_misc_data();
    if (misc == 0) {
        battle_menu_init_system_function(9, 3, unit->battle_data->misc_unit_id, 0, 1);
        return;
    }
    kind = unit->target_select_command;
    if (kind == 0xC) {
        battle_menu_init_system_function(9, 1, unit->battle_data->misc_unit_id, 0, 1);
        return;
    }
    if (kind == 0xE) {
        battle_menu_init_system_function(9, 2, unit->battle_data->misc_unit_id, 0, 1);
    }
}
