#include "fft/battle_action.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"

/*
 * Find an eligible unit occupying the requested map tile.
 *
 * The status filter bits are alternatives: a unit with any requested status
 * is eligible. The Chocobo bit is an additional requirement. Mounted riders
 * resolve to the mount's battle slot, and invalid coordinates or terrain are
 * distinguished from an empty tile.
 */
s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters) {
    /* A byte collision test retains the original single-mask branch. */
    s32 unit_index;
    unit_stats_t* unit;
    map_tile_t* tile;
    u8 mount_info;
    /* The unit loop tests a byte copy of the filters. */
    u8 unit_filters;

    unit_filters = filters;

    if (x < 0 || x >= g_map_max_x || y < 0 || y >= g_map_max_y || level >= 2) {
        return -2;
    }

    tile = &g_map_tile_data[(level << 8) + y * g_map_max_x + x];
    if (((*(u8*)&tile->flags) & MAP_TILE_COLLISION_MASK)) {
        return -2;
    }
    if (filters & BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE) {
        if (tile->depth_half_height.depth != 0) {
            return -2;
        }
        if (tile->depth_half_height.half_height >= 3) {
            return -2;
        }
    }

    for (unit_index = 0; unit_index < BATTLE_UNIT_SLOT_COUNT; unit_index++) {
        unit = &g_battle_unit_stats[unit_index];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE) {
            continue;
        }
        if ((unit_filters & BATTLE_UNIT_TILE_STATUS_FILTERS)
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_CRYSTAL)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
                    & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL)))
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_TREASURE)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                    & STATUS_BYTE_MASK(STATUS_ID_TREASURE)))
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_TRANSPARENT)
                && (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TRANSPARENT)]
                    & STATUS_BYTE_MASK(STATUS_ID_TRANSPARENT)))) {
            continue;
        }
        if ((unit_filters & BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO)
            && battle_unit_get_chocobo_mount_state(unit) == BATTLE_CHOCOBO_MOUNT_NONE) {
            continue;
        }
        if (unit->x != x || unit->position.y != y || unit->position.higher_elevation != level) {
            continue;
        }

        mount_info = unit->mount_info;
        if (mount_info >= BATTLE_MOUNT_INFO_FLAG_RIDER) {
            return mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        return unit_index;
    }

    return -1;
}
