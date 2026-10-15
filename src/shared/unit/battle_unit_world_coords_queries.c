#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* The number of records in the runtime unit list. No retail code calls it. */
s32 battle_unit_count(void) {
    s32 count = 0;
    battle_unit_t* unit = g_battle_unit_list_head;
    while (unit != 0) {
        unit = unit->previous;
        count++;
    }
    return count;
}

/* The integer world position of the unit with this runtime id, or 0 for a missing unit.
 * unit_copy_world_coords and unit_project_to_screen test
 * the result against -1 instead. */
SVECTOR* battle_unit_get_world_coords_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/* The integer world position of the unit in slot battle_id, or 0. No retail code calls
 * it. */
SVECTOR* battle_unit_get_world_coords_by_battle_id(u32 battle_id) {
    battle_unit_t* unit = battle_unit_get_by_battle_id(battle_id & 0xffff);

    if (unit == 0) {
        return 0;
    }
    return &unit->world_coords;
}

/*
 * Store a unit's tile-center coordinates and calculated screen Z.
 *
 * Return 0 when the battle ID has no renderer-side runtime record, otherwise 1.
 * The original performs a tile lookup whose returned pointer is unused; keep
 * that call because it is part of the target instruction stream.
 */
s32 battle_unit_get_tile_center_and_height_by_battle_id(u32 battle_id, SVECTOR* coords) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xffff);
    if (unit != 0) {
        map_get_tile_data_pointer(unit->stats->x, unit->stats->position.y, unit->stats->position.higher_elevation);
        coords->vx = unit->stats->x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vz = unit->stats->position.y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        coords->vy = battle_gfx_calculate_world_height_from_battle_tile(unit);
        return 1;
    }
    return 0;
}
