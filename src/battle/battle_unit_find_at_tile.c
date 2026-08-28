#include "fft/battle.h"
#include "fft/map.h"
#include "fft/unit_slots.h"

/*
 * Find an eligible unit occupying the requested map tile.
 *
 * The status filter bits are alternatives: a unit with any requested status
 * is eligible. The Chocobo bit is an additional requirement. Mounted riders
 * resolve to the mount's battle slot, and invalid coordinates or terrain are
 * distinguished from an empty tile.
 */
s32 battle_unit_find_at_tile(s32 x, s32 y, u32 level, s32 filters) {
    s32 unit_index;
    battle_stats_t* unit;
    volatile map_tile_t* tile;
    u8 mount_info;
    s32 tile_half_height;
    s32 unit_filters;
    /* Keep the pre-loop filter test in its incoming argument register. */
    register s32 dry_filters __asm__("$7");
    register s32 dry_mask __asm__("$2");

    unit_filters = filters;
    dry_filters = filters;

    if (x < 0 || x >= g_map_max_x || y < 0 || y >= g_map_max_y || level >= 2) {
        return -2;
    }

    tile = &g_battle_map_tile_data[(level << 8) + y * g_map_max_x + x];
    if (tile->flags_06.value & MAP_TILE_COLLISION_MASK) {
        return -2;
    }
    dry_mask = dry_filters & BATTLE_UNIT_TILE_FILTER_DRY_LOW_SLOPE;
    if (dry_mask) {
        if (tile->depth_half_height & MAP_TILE_DEPTH_MASK) {
            return -2;
        }
        /* The target reloads this byte after checking its high depth bits. */
        tile_half_height = tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK;
        if (tile_half_height >= 3) {
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
                && (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_CRYSTAL)]
                    & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CRYSTAL)))
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_TREASURE)
                && (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_TREASURE)]
                    & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)))
            && !((unit_filters & BATTLE_UNIT_TILE_FILTER_TRANSPARENT)
                && (unit->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_TRANSPARENT)]
                    & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TRANSPARENT)))) {
            continue;
        }
        if ((unit_filters & BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO) && battle_unit_check_chocobo(unit) == 0) {
            continue;
        }
        if (unit->x != x || unit->position.bits.y != y || unit->position.bits.higher_elevation != level) {
            continue;
        }

        mount_info = unit->mount_info;
        if ((u32)(mount_info & 0xff) >= BATTLE_MOUNT_INFO_FLAG_RIDER) {
            return mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        }
        return unit_index;
    }

    return -1;
}
