#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Convert map-tile coordinates to tile-centered Q12 world coordinates.
 *
 * Vertical position is derived separately from the unit's terrain and mount
 * state. The stack-backed SVECTOR preserves the target's narrow stores. */
void battle_unit_set_real_coords_from_map_coords(battle_unit_t* unit) {
    SVECTOR tile_center;

    tile_center.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->real.vx = tile_center.vx * ONE;
    tile_center.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
    unit->real.vz = tile_center.vz * ONE;
    unit->real.vy = battle_gfx_calculate_world_height_from_unit_tile(unit) * ONE;
}
