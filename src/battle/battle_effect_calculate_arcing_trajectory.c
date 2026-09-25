#include "fft/battle.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Solve the two launch slopes of an arcing projectile toward the target.
 *
 * Lowers the target's aim height from half, to three quarters, to zero of its
 * sprite height until both the arc apex and the target tile floor clear it,
 * storing the chosen height back into the target endpoint. Returns 0 when the
 * target is at zero distance or no height clears. */
s32 battle_effect_calculate_arcing_trajectory(battle_arc_query_t* query, s32* arc_a, s32* arc_b) {
    map_tile_t* tile;
    s32 tile_height;
    s32 floor;
    s32 peak;
    s32 height;
    s32 limit;
    s32 root;
    s32 gravity;
    s32 distance;
    s32 result;

    result = 0;
    tile = battle_map_get_tile_data_ptr_from_battle_id(query->target.unit_id);
    tile_height = tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT);
    floor = (tile_height * 3) << 14;
    peak = battle_effect_calculate_arc_height_term(query->delta.distance);
    if (query->delta.distance != 0) {
        height = query->target.height / 2;
        limit = (height + query->delta.y) << 12;
        if (peak >= limit && floor >= limit) {
            query->target.height = height;
        } else {
            height = query->target.height * 3 / 4;
            limit = (height + query->delta.y) << 12;
            if (peak >= limit && floor >= limit) {
                query->target.height = height;
            } else {
                limit = query->delta.y << 12;
                if (peak < limit || floor < limit) {
                    return result;
                }
                query->target.height = 0;
            }
        }
        gravity = g_battle_effect_gravity_modifier >> 6;
        distance = query->delta.distance >> 6;
        root = SquareRoot12((gravity - (limit >> 5)) * gravity - distance * distance);
        *arc_a = ((root + g_battle_effect_gravity_modifier) << 8) / (query->delta.distance >> 4);
        *arc_b = ((g_battle_effect_gravity_modifier - root) << 8) / (query->delta.distance >> 4);
        result = 1;
    }
    return result;
}
