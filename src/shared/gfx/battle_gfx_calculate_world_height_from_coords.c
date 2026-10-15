#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"

/*
 * Returns the signed-16-bit world height at coords after applying unit height,
 * terrain depth, movement behavior and sprite-specific adjustments.
 * vx/vz are integer world x/depth; vy selects the map layer.
 * An unavailable unit-height record falls back to terrain height.
 */
s32 battle_gfx_calculate_world_height_from_coords(battle_unit_t* unit, const SVECTOR* coords) {
    battle_unit_height_data_t height;
    map_tile_t* tile;
    s32 flags;
    s16 world_height;
    s32 sprite;
    s32 animation;
    s16 raised;

    if (unit->standing_on_battle_id_plus_one != 0) {
        battle_unit_calculate_height_data(&height, unit->standing_on_battle_id_plus_one - 1);
        if (height.unit_flags & BATTLE_UNIT_HEIGHT_UNAVAILABLE) {
            world_height = map_calculate_slope_height(coords, coords->vy & 0xff);
        } else {
            world_height = -height.total_height / 2 * 12;
        }
    } else {
        world_height = map_calculate_slope_height(coords, coords->vy & 0xff);
    }
    if (unit->stats != 0) {
        tile = map_get_tile_data_pointer(coords->vx / MAP_TILE_SIZE, coords->vz / MAP_TILE_SIZE, coords->vy);
        if (tile->depth_half_height.depth) {
            flags = move_get_effective_flags(unit->stats);
            sprite = unit->spritesheet_id;
            animation = unit->animation.sequence_id >> 1;
            if ((flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) || (flags & BATTLE_EFFECTIVE_MOVEMENT_ON_WATER)) {
                world_height -= (tile->depth_half_height.depth) * 12 + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_CHICKEN || sprite == BATTLE_SPRITESHEET_ID_FROG) {
                raised = world_height + 4;
                world_height = raised - ((tile->depth_half_height.depth) * 12 + unit->depth_height_offset);
            } else if ((flags & BATTLE_EFFECTIVE_MOVEMENT_IN_WATER) || animation == BATTLE_UNIT_ANIMATION_DEAD) {
                world_height -= ((tile->depth_half_height.depth) - 1) * 12 + unit->depth_height_offset;
            } else if (sprite == BATTLE_SPRITESHEET_ID_TREASURE || sprite == BATTLE_SPRITESHEET_ID_CRYSTAL) {
                world_height -= (tile->depth_half_height.depth) * 12 + unit->depth_height_offset;
            } else {
                world_height -= unit->depth_height_offset;
            }
        } else {
            world_height -= unit->depth_height_offset;
        }
    } else {
        world_height -= unit->depth_height_offset;
    }
    return world_height;
}
