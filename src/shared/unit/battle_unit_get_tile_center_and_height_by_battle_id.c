#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"

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
