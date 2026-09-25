/* The mounted-pair tail first uses $v1 for the partner ID, then reuses it for
 * the partner pointer.  Scoped bindings reproduce that short-lived allocation
 * without emitting instructions. */
#include "fft/battle.h"
#include "psx/types.h"

/* Move a unit to (x, y, level) with a new facing, carrying its mounted partner.
 *
 * A unit already standing on the destination becomes this unit's rider; any
 * stale mount link is dropped first.  Returns -1 without moving anything when
 * the destination tile is blocked or untargetable, 0 otherwise. */
s32 battle_unit_set_tile_position(s32 unit_id, u8 x, u8 y, u8 level, u8 facing) {
    battle_stats_t* unit;
    register battle_stats_t* partner __asm__("$3");
    s32 occupant;
    s32 rider;
    s32 has_rider;
    u8 mount_info;
    s32 rider_link;
    u8 tile_flags;

    has_rider = 0;
    unit = &g_battle_unit_stats[unit_id];
    occupant = battle_unit_find_at_tile(x, y, level, BATTLE_UNIT_TILE_FILTER_MOUNTABLE_CHOCOBO);
    if (occupant >= 0 && occupant != unit_id) {
        rider = occupant;
        has_rider = 1;
    }

    tile_flags = g_battle_map_tile_data[(level << 8) + y * g_map_max_x + x].flags_06.value;
    if (tile_flags & MAP_TILE_FLAG_BLOCKED) {
        return -1;
    }
    if (tile_flags & MAP_TILE_FLAG_UNTARGETABLE) {
        return -1;
    }

    mount_info = unit->mount_info;
    if (!(mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
        unit->mount_info = 0;
        if (mount_info != 0) {
            g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK].mount_info = 0;
        }
    }
    if (has_rider != 0) {
        /* The intermediate keeps the add in SImode; assigning the sum directly
         * narrows it to `addiu -0x80`, which the target does not use. */
        rider_link = rider + BATTLE_MOUNT_INFO_FLAG_RIDER;
        unit->mount_info = rider_link;
        g_battle_unit_stats[(u8)rider].mount_info = unit_id + BATTLE_MOUNT_INFO_FLAG_MOUNT;
    }

    unit->position.bits.y = y;
    unit->x = x;
    unit->position.bits.higher_elevation = level;
    unit->position.bits.facing = facing;

    if (mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT) {
        register s32 partner_index __asm__("$3");

        partner_index = mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK;
        /* Keeps the masked index in $v1; without it the index scaling runs
           with v0 and v1 swapped. */
        __asm__("" : "=r"(partner_index) : "0"(partner_index));
        partner = &g_battle_unit_stats[partner_index];
        partner->position.bits.y = y;
        partner->x = x;
        partner->position.bits.higher_elevation = level;
        partner->position.bits.facing = facing;
    }
    return 0;
}
