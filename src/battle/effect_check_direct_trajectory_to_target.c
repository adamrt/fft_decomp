#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"

/*
 * Sets up an arcing projectile trajectory between a source unit and either a
 * target unit or a bare tile, and reports whether the flight is obstructed.
 *
 * Both endpoints are raised by two thirds of their sprite height (12 when the
 * target has none), as in effect_check_direct_trajectory_between_units;
 * the tile case instead centers on the tile at tile_position (x, layer, y) and
 * sits on its surface, as in effect_set_arrow_trajectory.  The extra
 * work over those two is the launch angle: ratan2 of the vertical delta
 * against the horizontal distance, biased by 0x400 (a quarter turn) into
 * g_effect_arctan_angle_mod.  Returns target_id when the path is clear
 * and the blocking unit from g_effect_trajectory_hit_unit_id when it is not.
 */
s32 effect_check_direct_trajectory_to_target(s32 source_id, SVECTOR* tile_position, s32 target_id) {
    VECTOR delta;
    map_tile_t* tile;
    effect_obstacle_unit_list_t* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    g_effect_trajectory_source_id = source_id;
    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    if (target_id != -1) {
        target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 2 / 3;
        if (target_height == 0) {
            target_height = 12;
        }
    }
    effect_list_valid_unit_targeting_data(source_id, -1, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, &g_effect_trajectory_origin);
    g_effect_trajectory_origin.vy -= source_height;
    if (target_id == -1) {
        tile = map_get_tile_data_pointer(tile_position->vx, tile_position->vz, tile_position->vy);
        g_effect_trajectory_destination.vx = tile_position->vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vz = tile_position->vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        g_effect_trajectory_destination.vy = -(tile->height * 12);
    } else {
        battle_unit_get_tile_center_and_height_by_battle_id(target_id, &g_effect_trajectory_destination);
        g_effect_trajectory_destination.vy -= target_height;
    }
    setVector(&delta, g_effect_trajectory_destination.vx - g_effect_trajectory_origin.vx,
        g_effect_trajectory_destination.vy - g_effect_trajectory_origin.vy,
        g_effect_trajectory_destination.vz - g_effect_trajectory_origin.vz);
    g_effect_arctan_angle_mod = ratan2(delta.vy, SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz)) + 0x400;
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) * ONE);
    if (effect_trace_projectile_path(&delta, &g_effect_trajectory_origin, &distance, obstacles) != 0) {
        return target_id;
    }
    return g_effect_trajectory_hit_unit_id;
}
