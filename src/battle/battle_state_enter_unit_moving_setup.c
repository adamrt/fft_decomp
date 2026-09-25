#include "fft/battle.h"
#include "psx/types.h"

void battle_state_enter_unit_moving_setup(void) {
    battle_unit_misc_data_t* src;
    battle_unit_misc_data_t* cast;
    battle_unit_misc_data_t* at_tile;
    u8 path_count;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVING_SETUP;
    src = battle_unit_get_source_misc_data();
    cast = battle_unit_get_casting_misc_data();

    if (cast->mount_byte & 0x80) {
        battle_menu_init_system_function(
            2, 1, src->battle_data->misc_unit_id, 0, src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    path_count = cast->movement_path_count + 2;
    if (path_count < 2) {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            battle_menu_init_system_function(
                2, 0, src->battle_data->misc_unit_id, 0, src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
        battle_menu_init_system_function(
            0xD, 0, src->battle_data->misc_unit_id, 0, src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    at_tile
        = battle_unit_get_crystal_or_treasure_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (at_tile != 0) {
        if (at_tile->status_flags_5_6 & BATTLE_MISC_STATUS_CRYSTAL) {
            battle_menu_init_system_function(
                2, 4, src->battle_data->misc_unit_id, 0, src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
    }
    battle_menu_init_system_function(
        2, 0, src->battle_data->misc_unit_id, 0, src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}
