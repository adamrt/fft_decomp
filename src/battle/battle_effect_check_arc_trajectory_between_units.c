#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Test whether either arcing projectile path reaches the target unit.
 *
 * The arc counterpart of battle_effect_check_direct_trajectory_between_units: the
 * source is raised by two thirds of its sprite height, battle_effect_calculate_arcing_trajectory yields
 * the two candidate arcs for the offset, and each is traced against the
 * other units' positions. Returns 1 when an arc is clear. */
s32 battle_effect_check_arc_trajectory_between_units(u8 source_id, u8 target_id) {
    SVECTOR rotation;
    battle_arc_query_t query;
    battle_effect_obstacle_unit_list_t* obstacles;
    s32 arc_a;
    s32 arc_b;

    query.source.unit_id = source_id;
    query.target.unit_id = target_id;
    query.source.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(source_id) * 2 / 3;
    query.target.height = battle_gfx_get_unit_spritesheet_height_by_unit_id(target_id);
    battle_effect_list_valid_unit_targeting_data(source_id, target_id, &obstacles);
    battle_unit_get_tile_center_and_height_by_battle_id(source_id, (battle_screen_coords_t*)&query.source.position);
    query.source.position.vy -= query.source.height;
    battle_unit_get_tile_center_and_height_by_battle_id(target_id, (battle_screen_coords_t*)&query.target.position);
    query.delta.x = query.target.position.vx - query.source.position.vx;
    query.delta.z = query.target.position.vz - query.source.position.vz;
    query.delta.y = query.source.position.vy - query.target.position.vy;
    query.delta.distance = SquareRoot12((query.delta.x * query.delta.x + query.delta.z * query.delta.z) << 12);
    rotation.vy = ratan2(-query.delta.z, query.delta.x);
    if (battle_effect_calculate_arcing_trajectory(&query, &arc_a, &arc_b) != 0
        && (battle_effect_trace_arc_trajectory_path(
                &query.source.position, &arc_a, &query.delta.distance, (battle_effect_arc_t*)&rotation, obstacles)
                != 0
            || battle_effect_trace_arc_trajectory_path(
                   &query.source.position, &arc_b, &query.delta.distance, (battle_effect_arc_t*)&rotation, obstacles)
                != 0)) {
        return 1;
    }
    return 0;
}
