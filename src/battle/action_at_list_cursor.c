#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_target.h"

/* Show the map cursor and its tile glow: the flag is read only by
 * target_calculate_tile_coords_and_glow_from_at_list. */
void action_set_at_list_active(void) {
    g_action_at_list_active = 1;
}

/* Hide the map cursor and its tile glow: the flag is read only by
 * target_calculate_tile_coords_and_glow_from_at_list. */
void action_clear_at_list_active(void) {
    g_action_at_list_active = 0;
}

void target_move_cursor_to_unit(const battle_unit_t* unit) {
    if (unit != 0) {
        g_battle_cursor_x = unit->map_x;
        g_battle_cursor_y = unit->map_y;
        g_battle_cursor_z = unit->map_z;
    }
}

/* Move the cursor to the target of the unit's stored action: the target tile, or
 * the tile of the target unit. */
void target_move_cursor_to_action_target(const battle_unit_t* unit) {
    unit_stats_t* bd;

    bd = unit->stats;
    if (bd->action_command.targeting_type == BATTLE_TARGETING_TYPE_TILE) {
        g_battle_cursor_x = bd->action_command.target_position.vx;
        g_battle_cursor_z = unit->stats->action_command.target_position.vy;
        g_battle_cursor_y = unit->stats->action_command.target_position.vz;
        return;
    }
    target_move_cursor_to_unit(battle_unit_get_by_battle_id(bd->action_command.target_id));
}
