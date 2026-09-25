#include "fft/battle.h"
#include "psx/gte.h"

/* Collects every other active unit's tile position into a scratchpad list. */
struct battle_effect_obstacle_unit_list;

/*
 * Trace a projectile path from one unit's upper body to another's.
 *
 * Both endpoints are raised by two thirds of the unit's sprite height (12 when
 * the target has none). `unused_20` and `unused_38` reproduce unreferenced
 * frame slots the target reserves between the vectors.
 */
s32 battle_effect_check_direct_trajectory_between_units(u8 source_id, u8 target_id) {
    SVECTOR origin;
    SVECTOR destination;
    SVECTOR unused_20;
    VECTOR delta;
    s32 unused_38[2];
    void* obstacles;
    s32 distance;
    s32 source_height;
    s32 target_height;

    source_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    target_height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id) * 2 / 3;
    if (target_height == 0) {
        target_height = 12;
    }
    battle_effect_list_valid_unit_targeting_data(
        source_id, target_id, (struct battle_effect_obstacle_unit_list**)&obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, (battle_screen_coords_t*)&origin);
    origin.vy -= source_height;
    battle_unit_get_tile_center_and_height_by_battle_id(target_id, (battle_screen_coords_t*)&destination);
    destination.vy -= target_height;
    delta.vx = destination.vx - origin.vx;
    delta.vy = destination.vy - origin.vy;
    delta.vz = destination.vz - origin.vz;
    distance = SquareRoot12((delta.vx * delta.vx + delta.vy * delta.vy + delta.vz * delta.vz) << 12);
    return battle_effect_trace_projectile_path(&delta, &origin, &distance, obstacles);
}
