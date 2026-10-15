#include "fft/battle_map.h"
#include "fft/battle_unit.h"

/* Compute unit height after mount, Float, and water movement adjustments.
 *
 * Float and BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER stand on the water surface;
 * BATTLE_MOVEMENT_SET_3_WALK_ON_WATER stands one level below it. */
u8 battle_unit_get_effective_height(const unit_stats_t* unit) {
    map_tile_t* tile;
    map_tile_depth_t terrain_depth;
    s32 height;
    u8 effective_height;
    u8 water_depth;
    u8 ignores_water;
    u8 flags;

    ignores_water = 0;
    tile = &g_map_tile_data[map_calculate_location(unit)];
    terrain_depth = tile->depth_half_height;
    flags = unit->mount_info;
    height = tile->height * 2 + (terrain_depth.half_height);
    water_depth = tile->depth_half_height.depth * 2;
    effective_height = height;
    if (flags & BATTLE_MOUNT_INFO_FLAG_RIDER) {
        unit = &g_battle_unit_stats[flags & 0x1f];
        height += 2;
        effective_height = height;
    }
    flags = unit->ability_flags.movement[2];
    if ((flags & BATTLE_MOVEMENT_SET_3_FLOAT)
        || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT))) {
        effective_height += 2;
        ignores_water = 1;
    }
    if (water_depth != 0) {
        if (ignores_water || (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER)) {
            effective_height += water_depth;
        } else if (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
            effective_height += water_depth - 2;
        }
    }
    return effective_height;
}
