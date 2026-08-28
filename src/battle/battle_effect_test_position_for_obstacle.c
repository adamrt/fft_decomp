#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/map.h"
#include "fft/unit_slots.h"
#include "psx/types.h"

/*
 * Test a projectile position against the terrain and the listed units.
 *
 * Fills `tile_ref` with the map tile under `position` and returns 1 when the
 * position is inside that tile's solid column (either map layer) or inside a
 * listed unit's sprite box; the blocking unit's battle id is reported through
 * `out_unit`, which is otherwise left at -1.
 *
 * The ceiling-depth byte is read through a volatile lvalue: the target reloads
 * it in each arm of the depth selection instead of reusing the tested value.
 */
s32 battle_effect_test_position_for_obstacle(
    battle_effect_obstacle_unit_list_t* list, VECTOR* position, s32* out_unit, battle_effect_tile_ref_t* tile_ref) {
    map_tile_t* tile;
    s32 ceiling;
    s32 height;
    s32 y;
    s32 i;

    *out_unit = -1;
    tile_ref->x = position->vx / 28;
    tile_ref->y = position->vz / 28;
    tile = battle_map_get_tile_data_pointer(tile_ref->x, tile_ref->y, 0);
    if (((*(volatile u8*)&tile->ceiling_depth_and_marks) & MAP_TILE_CEILING_DEPTH_MASK) == 0) {
        ceiling = tile->height;
    } else {
        ceiling = (*(volatile u8*)&tile->ceiling_depth_and_marks) & MAP_TILE_CEILING_DEPTH_MASK;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * 12) && y < -((height - ceiling) * 12)) {
        tile_ref->layer = 0;
        g_battle_effect_trajectory_tile_flags = tile->flags_06.bits.unknown_4_6;
        return 1;
    }

    tile = battle_map_get_tile_data_pointer(tile_ref->x, tile_ref->y, 1);
    ceiling = 1;
    if (((*(volatile u8*)&tile->ceiling_depth_and_marks) & MAP_TILE_CEILING_DEPTH_MASK) != 0) {
        ceiling = (*(volatile u8*)&tile->ceiling_depth_and_marks) & MAP_TILE_CEILING_DEPTH_MASK;
    }
    height = tile->height;
    y = position->vy;
    if (y >= -(height * 12) && y < -((height - ceiling) * 12)) {
        tile_ref->layer = 1;
        g_battle_effect_trajectory_tile_flags = tile->flags_06.bits.unknown_4_6;
        return 1;
    }

    for (i = 0; i < list->count; i++) {
        if (position->vy < list->positions[i].vy && list->positions[i].vy - list->sprite_heights[i] < position->vy
            && list->sprite_heights[i] != 0 && list->positions[i].vx - 8 < position->vx
            && position->vx < list->positions[i].vx + 8 && list->positions[i].vz - 8 < position->vz
            && position->vz < list->positions[i].vz + 8) {
            *out_unit = list->battle_ids[i];
            return 1;
        }
    }
    return 0;
}
