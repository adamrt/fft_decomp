#include "fft/battle.h"
#include "psx/types.h"

/* The callee sign-extends its result, yet battle_move_step_unit_to_map_tile_center needs an s32 return and
 * battle_move_animate_jump_arc_to_target an s16 second argument: no shared prototype yet. */

/*
 * Move a Misc unit_t to the map tile at tile[0..2] (x, z, y) facing `facing`.
 *
 * Clears the effect vectors, sets both the current and destination tile, and
 * recomputes the screen and fixed-point world positions from the tile.
 */
s32 battle_unit_place_in_new_location(u32 misc_id, s16* tile, s32 facing) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_misc_id(misc_id & 0xFFFF);
    if (unit != 0) {
        unit->effect_vector_2.vz = 0;
        unit->effect_vector_2.vy = 0;
        unit->effect_vector_2.vx = 0;
        unit->effect_vector.vz = 0;
        unit->effect_vector.vy = 0;
        unit->effect_vector.vx = 0;
        unit->screen_offset.vz = 0;
        unit->screen_offset.vy = 0;
        unit->screen_offset.vx = 0;
        unit->map_x = unit->movement.bytes.destination_x = tile[0];
        unit->map_y = unit->movement.bytes.destination_y = tile[2];
        unit->map_z = unit->movement.bytes.destination_z = tile[1];
        unit->facing = facing << 10;
        unit->screen.vx = tile[0] * 28 + 14;
        unit->screen.vz = tile[2] * 28 + 14;
        unit->screen.vy = battle_map_calculate_slope_height(&unit->screen.vx, unit->map_z);
        unit->real.vx = unit->screen.vx << 12;
        unit->real.vz = unit->screen.vz << 12;
        unit->real.vy = unit->screen.vy << 12;
        if (unit->battle_data != 0) {
            battle_unit_set_tile_position(
                unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z, unit->facing);
        }
        return 1;
    }
    return 0;
}
