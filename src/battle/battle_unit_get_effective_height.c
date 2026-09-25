#include "fft/battle.h"

/* Compute unit height after mount, Float, and water movement adjustments.
 *
 * Fly, Teleport, Float, and Walk on Water stand above water; Swim lowers a
 * unit into it. */
u8 battle_unit_get_effective_height(battle_stats_t* unit) {
    map_tile_t* tile;
    s32 height;
    u8 effective_height;
    u8 water_depth;
    u8 ignores_water;
    u8 flags;

    ignores_water = 0;
    tile = &g_battle_map_tile_data[battle_map_calculate_location(unit)];
    flags = unit->mount_info;
    height = tile->height * 2 + (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK);
    water_depth = (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT) * 2;
    effective_height = height;
    if (flags & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_battle_unit_stats[flags & 0x1f];
        height += 2;
        effective_height = height;
    }
    flags = unit->movement_abilities[2];
    if ((flags & BATTLE_MOVEMENT_SET_3_FLOAT)
        || (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT))) {
        effective_height += 2;
        ignores_water = 1;
    }
    if (water_depth != 0) {
        if (ignores_water || (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER)) {
            effective_height += water_depth;
        } else if (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
            effective_height += water_depth - 2;
        }
    }
    return effective_height;
}
