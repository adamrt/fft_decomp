#include "fft/battle.h"
#include "psx/types.h"

void battle_move_update_unit_step_to_destination_tile_entry_edge(battle_unit_misc_data_t* unit) {
    s32 direction;

    direction = battle_move_get_direction(unit);
    battle_move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        battle_move_displace_unit_at_destination_tile(unit, direction);
    }
    if (battle_move_has_reached_destination_tile_entry_edge(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        if (unit->screen.vy >= (s16)battle_gfx_calculate_screen_z_from_misc_screen_data(unit)) {
            unit->velocity.vy = 0;
            battle_move_start_float_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
            unit->centre_tile_offset = g_battle_move_entry_edge_centre_offsets[direction];
        }
    }
}
