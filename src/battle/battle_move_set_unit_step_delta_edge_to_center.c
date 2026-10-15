#include "fft/battle_move.h"
#include "psx/types.h"

enum {
    MOVE_STEP_HEIGHT_SCALE = 6,
};

/* Set the unit's per-frame velocity for the second half of a step, from the entry
 * edge of a tile to its center.
 *
 * The mirror of move_set_unit_step_delta_center_to_edge: the height part
 * of the direction vector, (edge_height - 1) * the tile's slope half-height * 6,
 * is not negated, so entering over a high side (slope height 2) moves the unit
 * down to the center. */
void battle_move_set_unit_step_delta_edge_to_center(
    battle_unit_t* unit, const move_step_t* step, const map_tile_t* tile, s32 edge_height) {
    VECTOR direction;

    direction.vy = (edge_height - 1) * tile->depth_half_height.half_height * MOVE_STEP_HEIGHT_SCALE;
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
