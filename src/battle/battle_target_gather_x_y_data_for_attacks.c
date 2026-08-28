#include "fft/battle.h"
#include "psx/types.h"

void battle_target_gather_x_y_data_for_attacks(battle_unit_misc_data_t* unit) {
    battle_stats_t* bd;

    bd = unit->battle_data;
    if (bd->action_target_kind == BATTLE_ACTION_TARGET_TILE) {
        g_battle_cursor_x = bd->action_target_x;
        g_battle_cursor_z = unit->battle_data->action_target_elevation;
        g_battle_cursor_y = unit->battle_data->action_target_y;
        return;
    }
    battle_target_move_cursor_to_unit(battle_unit_get_misc_data_by_battle_id(bd->action_target_id));
}
