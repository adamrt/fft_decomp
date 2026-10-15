#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Move a unit to (x, y, level) with a new facing, carrying its mounted partner.
 *
 * A unit already standing on the destination becomes this unit's rider; any
 * stale mount link is dropped first.  Returns -1 without moving anything when
 * the destination tile is blocked or untargetable, 0 otherwise. */
s32 battle_unit_set_tile_position(s32 battle_id, u8 x, u8 y, u8 level, u8 facing) {
    unit_stats_t* unit;
    unit_stats_t* partner;
    s32 occupant;
    u8 rider;
    s32 has_rider;
    u8 mount_info;
    map_tile_flags_t tile_flags;

    has_rider = 0;
    unit = &g_battle_unit_stats[battle_id];
    occupant = battle_unit_find_at_tile(x, y, level, BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO);
    if (occupant >= 0 && occupant != battle_id) {
        rider = occupant;
        has_rider = 1;
    }

    tile_flags = g_map_tile_data[(level << 8) + y * g_map_max_x + x].flags;
    if (tile_flags.blocked) {
        return -1;
    }
    if (tile_flags.untargetable) {
        return -1;
    }

    mount_info = unit->mount_info;
    if (!(mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
        unit->mount_info = 0;
        if (mount_info != 0) {
            partner = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
            partner->mount_info = 0;
        }
    }
    if (has_rider != 0) {
        unit->mount_info = rider + BATTLE_MOUNT_INFO_FLAG_RIDER;
        g_battle_unit_stats[rider].mount_info = battle_id + BATTLE_MOUNT_INFO_FLAG_MOUNT;
    }

    unit->position.y = y;
    unit->x = x;
    unit->position.higher_elevation = level;
    unit->position.facing = facing;

    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        partner = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
        partner->position.y = y;
        partner->x = x;
        partner->position.higher_elevation = level;
        partner->position.facing = facing;
    }
    return 0;
}
