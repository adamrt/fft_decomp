#include "fft/battle.h"
#include "psx/types.h"

void battle_action_get_next_acting_unit(s32 at_index) {
    s32 id;
    battle_unit_misc_data_t* unit;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_ACTIVE_TURN;
    id = battle_action_calculate_at_list_and_get_specific_unit_id(at_index);
    if (id >= 0) {
        unit = battle_unit_get_misc_data_by_battle_id(id & 0xFF);
        if ((id & 0x100) != 0) {
            battle_target_gather_x_y_data_for_attacks(unit);
            battle_target_calculate_for_menu_types(&unit->battle_data->action_actor_id);
            battle_target_set_tile_background_color(7, 3);
        } else {
            battle_target_move_cursor_to_unit(unit);
            battle_target_set_tile_background_color(0, 0);
        }
    }
    battle_target_store_cursor_unit_name_and_data();
    battle_action_set_at_list_active();
}
