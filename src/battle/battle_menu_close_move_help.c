#include "fft/battle.h"
#include "psx/types.h"

void battle_menu_close_move_help(void) {
    battle_unit_misc_data_t* unit;
    battle_unit_misc_data_t* casting;

    battle_state_stop_game_flow();
    unit = battle_unit_get_source_misc_data();
    casting = battle_unit_get_casting_misc_data();
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) != 0) {
        battle_action_set_at_list_active();
    } else {
        battle_action_clear_at_list_active();
    }

    if (battle_move_set_reachable_tiles(
            casting->battle_data->misc_unit_id, casting->map_x, casting->map_y, casting->map_z)
        > 0) {
        battle_target_set_tile_background_color(1, 1);
        g_battle_game_state = BATTLE_GAME_STATE_CLOSE_MOVE_HELP;
        casting->state_frame_counter = 0;
        battle_target_store_cursor_unit_name_and_data();
    } else {
        battle_action_enter_move_range_exception();
    }
}
