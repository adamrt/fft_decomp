#include "fft/battle_map.h"
#include "fft/battle_unit.h"

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
battle_unit_height_data_t* battle_unit_calculate_height_data(battle_unit_height_data_t* out, s32 unit_id) {
    battle_unit_height_data_t height;
    unit_stats_t* unit;
    unit_stats_t* candidate;
    map_tile_t* tile;
    /* Byte access retains the original packed load and field-extraction order. */
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
    if (unit_id >= 0 && unit_id < BATTLE_UNIT_SLOT_COUNT && (id = unit->entd_slot) != 0xff
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)] & STATUS_BYTE_MASK(STATUS_ID_CRYSTAL))
        && !(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)] & STATUS_BYTE_MASK(STATUS_ID_TREASURE))) {
        flags = unit_id;
        id = unit->job_id;
        if (id < JOB_ID_CHOCOBO || id > JOB_ID_RED_CHOCOBO)
            flags = unit_id | BATTLE_UNIT_HEIGHT_NON_CHOCOBO;
        x = unit->x;
        height.x = x;
        y = unit->position.y;
        height.y = y;
        level = unit->position.higher_elevation;
        height.elevation = level;
        for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
            candidate = &g_battle_unit_stats[i];
            if (i != unit_id && candidate->entd_slot != BATTLE_ENTD_SLOT_NONE && candidate->x == x
                && candidate->position.y == y && candidate->position.higher_elevation == level) {
                if (flags & BATTLE_UNIT_HEIGHT_NON_CHOCOBO)
                    unit = candidate;
                offset = 2;
                flags |= BATTLE_UNIT_HEIGHT_SHARED_TILE;
                break;
            }
        }
        height.unit_flags = flags;
        flags = unit->status_sets.current[2];
        if (flags & (STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG)))
            height.unit_height = offset + 4;
        else
            height.unit_height = offset + 6;
        flags = unit->ability_flags.movement[2];
        tile = &g_map_tile_data[map_calculate_location(unit)];
        tile_height = tile->height * 2 + ((*(u8*)&tile->depth_half_height) & MAP_TILE_HALF_HEIGHT_MASK);
        depth = (*(u8*)&tile->depth_half_height) >> MAP_TILE_DEPTH_SHIFT;
        level = tile_height;
        if (depth != 0) {
            if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
                water_offset = depth * 2 + 2;
                level = tile_height + water_offset;
            } else if (flags & BATTLE_MOVEMENT_SET_3_MOVE_IN_WATER) {
                level = tile_height + depth * 2;
            } else if (flags & BATTLE_MOVEMENT_SET_3_WALK_ON_WATER) {
                water_offset = depth * 2 - 2;
                level = tile_height + water_offset;
            }
        } else if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_FLOAT)] & STATUS_BYTE_MASK(STATUS_ID_FLOAT)) {
            level = tile_height + 2;
        }
        height.walking_height = level;
        height.total_height = level + height.unit_height;
        if (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_DEAD)] & STATUS_BYTE_MASK(STATUS_ID_DEAD))
            height.unit_flags = BATTLE_UNIT_HEIGHT_UNAVAILABLE;
    } else {
        height.unit_flags = BATTLE_UNIT_HEIGHT_UNAVAILABLE;
    }
    *out = height;
    return out;
}
