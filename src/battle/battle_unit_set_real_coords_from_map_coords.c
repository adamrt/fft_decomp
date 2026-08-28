#include "fft/battle.h"

/* Convert map-tile coordinates to tile-centered Q12 world coordinates.
 *
 * Vertical position is derived separately from the unit's terrain and mount
 * state. The stack-backed SVECTOR preserves the target's narrow stores. */
void battle_unit_set_real_coords_from_map_coords(battle_unit_misc_data_t* unit) {
    SVECTOR tile_center;

    tile_center.vx = unit->map_x * 0x1C + 0xE;
    unit->real.vx = (s32)tile_center.vx << 12;
    tile_center.vz = unit->map_y * 0x1C + 0xE;
    unit->real.vz = (s32)tile_center.vz << 12;
    unit->real.vy = battle_gfx_calculate_screen_z_from_misc_map_data(unit) << 12;
}
