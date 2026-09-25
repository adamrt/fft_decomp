#include "fft/battle.h"
#include "fft/job.h"
#include "fft/map.h"

/* The result bytes as the target writes them. The elevation store is a 7-bit
 * field write that keeps bit 7 of the uninitialized local byte, and only these
 * 7 bytes are copied out. */
typedef struct battle_unit_height_copy {
    u8 x;
    u8 y;
    u8 elevation : 7;
    u8 unknown_bit7 : 1;
    u8 unit_flags;
    s8 unit_height;
    s8 walking_height;
    s8 total_height;
} battle_unit_height_copy_t;

/* Build a unit's targeting-height record.
 *
 * For a non-Chocobo, another unit at the same x, y, and elevation supplies
 * terrain, status, and movement data. Body, Float, and water movement then
 * determine the effective height.
 *
 * `id` holds the ENTD slot and then the job, and `level` the elevation and
 * then the walking height. Each shared variable is required: `id` conflicts
 * with the 0xff constant in $v0, which puts the job load in $v1, and `level`
 * outranks `depth` in allocation, which gives it $v1 and `depth` $a1.
 */
battle_unit_height_data_t* battle_calculate_unit_height_data(battle_unit_height_data_t* out, s32 unit_id) {
    battle_unit_height_copy_t height;
    battle_stats_t* unit;
    battle_stats_t* candidate;
    map_tile_t* tile;
    s32 offset;
    s32 i;
    s32 id;
    u8 flags;
    u8 x;
    u8 y;
    u8 level;
    s32 tile_height;
    s32 depth;
    s32 water_offset;

    offset = 0;
    unit = &g_battle_unit_stats[unit_id];
    if ((u32)unit_id < BATTLE_UNIT_SLOT_COUNT && (id = unit->entd_slot) != 0xff
        && (*(u16*)&unit->status_sets.current[0]
               & (BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_CRYSTAL)
                   | BATTLE_STATUS_PACKED_MASK(BATTLE_STATUS_ID_TREASURE)))
            == 0) {
        flags = unit_id;
        id = unit->job_id;
        if ((u32)(id - JOB_ID_CHOCOBO) >= JOB_ID_CHOCOBO_FAMILY_COUNT)
            flags = unit_id | BATTLE_UNIT_HEIGHT_NON_CHOCOBO;
        x = unit->x;
        height.x = x;
        y = unit->position.bits.y;
        height.y = y;
        level = unit->position.bits.higher_elevation;
        height.elevation = level;
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            candidate = &g_battle_unit_stats[i];
            if (i != unit_id && candidate->entd_slot != BATTLE_ENTD_SLOT_NONE && candidate->x == x
                && candidate->position.bits.y == y && candidate->position.bits.higher_elevation == level) {
                if (flags & BATTLE_UNIT_HEIGHT_NON_CHOCOBO)
                    unit = candidate;
                offset = 2;
                flags |= BATTLE_UNIT_HEIGHT_SHARED_TILE;
                break;
            }
        }
        height.unit_flags = flags;
        flags = unit->status_sets.current[2];
        if (flags
            & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)))
            height.unit_height = offset + 4;
        else
            height.unit_height = offset + 6;
        flags = unit->movement_abilities[2];
        tile = &g_battle_map_tile_data[battle_map_calculate_location(unit)];
        tile_height = tile->height * 2 + (tile->depth_half_height & MAP_TILE_HALF_HEIGHT_MASK);
        depth = tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT;
        level = tile_height;
        if (depth != 0) {
            if (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)) {
                water_offset = depth * 2 + 2;
                level = tile_height + water_offset;
            } else if (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
                level = tile_height + depth * 2;
            } else if (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
                water_offset = depth * 2 - 2;
                level = tile_height + water_offset;
            }
        } else if (unit->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FLOAT)) {
            level = tile_height + 2;
        }
        height.walking_height = level;
        height.total_height = level + height.unit_height;
        if (unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_DEAD))
            height.unit_flags = BATTLE_UNIT_HEIGHT_UNAVAILABLE;
    } else {
        height.unit_flags = BATTLE_UNIT_HEIGHT_UNAVAILABLE;
    }
    *(battle_unit_height_copy_t*)out = height;
    return out;
}
