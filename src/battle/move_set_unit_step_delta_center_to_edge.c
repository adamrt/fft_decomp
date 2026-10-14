#include "fft/battle_move.h"
#include "psx/types.h"

/* Set the unit's per-frame velocity for the first half of a step, from the tile
 * center to its exit edge.
 *
 * The direction vector is 28 along the axis the step byte's top two bits select
 * and (edge_height - 1) * the tile's slope half-height * 6 in height, negated
 * because height is negative upward. edge_height is the slope_type height
 * of the side crossed (0 low, 1 sloping, 2 high), so a low side
 * lies below the center and a high side above it. VectorNormal reduces the vector
 * to unit length (ONE) and g_main_system_animation_speed scales it; the caller
 * applies the walk speed with move_interpolate_partial. */
void move_set_unit_step_delta_center_to_edge(
    battle_unit_t* unit, const move_step_t* step, const map_tile_t* tile, s32 edge_height) {
    VECTOR direction;

    direction.vy = -((edge_height - 1) * tile->depth_half_height.half_height * (MAP_HEIGHT_STEP / 2));
    switch (step->direction) {
    case MOVE_STEP_DIRECTION_EAST:
        direction.vx = MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        direction.vx = -MAP_TILE_SIZE;
        direction.vz = 0;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        direction.vx = 0;
        direction.vz = -MAP_TILE_SIZE;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        direction.vx = 0;
        direction.vz = MAP_TILE_SIZE;
        break;
    }
    VectorNormal(&direction, &unit->velocity);
    unit->velocity.vx = unit->velocity.vx * g_main_system_animation_speed;
    unit->velocity.vy = unit->velocity.vy * g_main_system_animation_speed;
    unit->velocity.vz = unit->velocity.vz * g_main_system_animation_speed;
}
