#include "fft/battle.h"

/*
 * Store a unit's tile-center coordinates and calculated screen Z.
 *
 * Return 0 when the battle ID has no renderer-side Misc record, otherwise 1.
 * The original performs a tile lookup whose returned pointer is unused; keep
 * that call because it is part of the target instruction stream.
 */
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, battle_screen_coords_t* coords) {
    battle_unit_misc_data_t* unit;

    unit = battle_unit_get_misc_data_by_battle_id(battle_id & 0xffff);
    if (unit != 0) {
        battle_map_get_tile_data_pointer(
            unit->battle_data->x, unit->battle_data->position.bits.y, unit->battle_data->position.raw >> 15);
        coords->x = unit->battle_data->x * 28 + 14;
        coords->y = unit->battle_data->position.bits.y * 28 + 14;
        coords->z = battle_gfx_calculate_screen_z_from_misc_battle_map_data(unit);
        return 1;
    }
    return 0;
}
