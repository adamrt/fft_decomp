#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/* Advance a moving unit by one frame of its step: add the per-frame velocity to
 * its real (Q12) position, keep the height consistent with the ground for the
 * stage in step_phase, and refresh the integer world coordinates. A mount also places
 * its rider, 10 world units above it.
 *
 * The tile under the new position is real / 0x1c000 (28 world units per tile);
 * on entering the destination tile the unit takes the destination's layer. Height
 * is negative upward. ground_height and destination_height are the integer world
 * Y heights under the unit and at its destination, including rider adjustments. Per stage:
 * - walk and climb, to the edge or to the center: at or below the ground the unit
 *   snaps onto it and the horizontal delta is rebuilt for the slope (the climb
 *   stages first raise step_speed by g_move_jump_gravity, up to 0x7fff).
 *   Above the ground it snaps when less than 6 units up, otherwise it gains
 *   gravity; 0x2a units or more above the ground (the destination's ground in
 *   the to-center stages) turns the step into a jump, with the fall animation
 *   outside action execution.
 * - jump ascent and descent, float jump: gravity, stopping at the ground while
 *   the unit moves down.
 * - float: at the ground, snap and rebuild the float delta; above it the fall
 *   speed gains gravity while it is under 0x2000.
 * - fly: at the ground, rebuild the velocity for the run of contiguous steps.
 *
 * The first switch repeats the layer test that the statement before it has just
 * made false, so its bodies never run. */
void move_apply_unit_step_velocity(battle_unit_t* unit) {
    s16 tile[2];
    u8 layer;
    s16 ground_height;
    s16 destination_height;
    map_tile_t* map_tile;
    battle_unit_t* rider;
    s32 depth;

    tile[0] = (unit->real.vx + unit->velocity.vx) / (MAP_TILE_SIZE * ONE);
    tile[1] = (unit->real.vz + unit->velocity.vz) / (MAP_TILE_SIZE * ONE);
    layer = unit->map_z;
    if (tile[0] == unit->movement.destination_x && tile[1] == unit->movement.destination_y
        && unit->map_z != unit->movement.destination_z) {
        layer = unit->map_z = unit->movement.destination_z;
    }
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
        if (tile[0] == unit->movement.destination_x && tile[1] == unit->movement.destination_y
            && unit->map_z != unit->movement.destination_z) {
            layer = unit->map_z = unit->movement.destination_z;
            move_set_unit_step_delta_edge_to_center(unit, &unit->current_path_step,
                map_get_tile_data_pointer(tile[0], tile[1], layer), unit->destination_edge_height);
            move_interpolate_partial(unit, unit->walk_speed);
        }
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        if (tile[0] == unit->movement.destination_x && tile[1] == unit->movement.destination_y
            && unit->map_z != unit->movement.destination_z) {
            layer = unit->map_z = unit->movement.destination_z;
            move_set_unit_step_delta_edge_to_center(unit, &unit->current_path_step,
                map_get_tile_data_pointer(tile[0], tile[1], layer), unit->destination_edge_height);
            move_interpolate_partial(unit, unit->step_speed);
        }
        break;
    }
    unit->real.vx += unit->velocity.vx;
    unit->real.vz += unit->velocity.vz;
    unit->world_coords.vx = unit->real.vx / ONE;
    unit->world_coords.vz = unit->real.vz / ONE;
    ground_height = battle_gfx_calculate_world_height_from_world_coords(unit);
    destination_height = battle_gfx_calculate_world_height_from_move_destination(unit);
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = map_get_tile_data_pointer(tile[0], tile[1], layer);
        if ((-unit->real.vy / ONE) <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_set_unit_step_delta_center_to_edge(
                unit, &unit->current_path_step, map_tile, unit->current_edge_height);
            move_interpolate_partial(unit, unit->walk_speed);
        } else {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -ground_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = map_get_tile_data_pointer(tile[0], tile[1], layer);
        if ((-unit->real.vy / ONE) <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_move_jump_gravity;
            }
            move_set_unit_step_delta_center_to_edge(
                unit, &unit->current_path_step, map_tile, unit->current_edge_height);
            move_interpolate_partial(unit, unit->step_speed);
        } else {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -ground_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_ASCENT_NORTH;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = map_get_tile_data_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_set_unit_step_delta_edge_to_center(
                unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
            move_interpolate_partial(unit, unit->walk_speed);
        }
        if (-ground_height < (-unit->real.vy / ONE)) {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -destination_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_DESCENT_NORTH;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = map_get_tile_data_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            if (unit->step_speed <= 0x7FFF) {
                unit->step_speed += g_move_jump_gravity;
            }
            move_set_unit_step_delta_edge_to_center(
                unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
            move_interpolate_partial(unit, unit->step_speed);
        }
        if ((-unit->real.vy / ONE) > -ground_height) {
            if ((-unit->real.vy / ONE) < -ground_height + 6) {
                unit->real.vy = ground_height * ONE;
            } else if (unit->velocity.vy <= 0x7FFF) {
                unit->velocity.vy += g_move_jump_gravity;
            }
            if (-unit->real.vy / ONE >= -destination_height + 0x2A) {
                unit->step_phase = MOVE_STEP_PHASE_JUMP_DESCENT_NORTH;
                if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
                    battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
                }
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        if (unit->velocity.vy <= 0x7FFF) {
            unit->velocity.vy += g_move_jump_gravity;
        }
        unit->real.vy += unit->velocity.vy;
        if (unit->velocity.vy > 0) {
            if (-unit->real.vy / ONE < -ground_height) {
                unit->real.vy = ground_height * ONE;
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = map_get_tile_data_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_set_float_step_delta_center_to_edge(
                unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
            move_interpolate_partial(unit, unit->walk_speed);
        } else {
            /* Shared with the next case: a duplicated branch does not merge. */
            goto accelerate;
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_tile = map_get_tile_data_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_set_float_step_delta_edge_to_center(
                unit, &unit->current_path_step, map_tile, unit->destination_edge_height);
            move_interpolate_partial(unit, unit->walk_speed);
        } else {
        accelerate:
            /* Keeps depth live here, so it takes a0 and reorg leaves `move a0,s0` out of both landing tests. */
            __asm__("" : : "r"(depth));
            if (unit->velocity.vy < 0x2000) {
                unit->velocity.vy += g_move_jump_gravity;
            }
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        unit->real.vy += unit->velocity.vy;
        map_get_tile_data_pointer(tile[0], tile[1], layer);
        if (-unit->real.vy / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_set_velocity_for_contiguous_clamped_steps(unit, &unit->path, &unit->current_path_step);
            move_interpolate_partial(unit, unit->walk_speed);
        }
        unit->shadow_dirty |= 1;
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        unit->real.vy += unit->velocity.vy;
        map_get_tile_data_pointer(tile[0], tile[1], layer);
        depth = -unit->real.vy;
        if (depth / ONE <= -ground_height) {
            unit->real.vy = ground_height * ONE;
            move_set_velocity_for_contiguous_steps_with_final_tile_height(unit, &unit->path, &unit->current_path_step);
            move_interpolate_partial(unit, unit->walk_speed);
        }
        unit->shadow_dirty |= 1;
        break;
    }
    battle_unit_set_world_coords_from_real_coords(unit);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        rider = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (rider != 0) {
            rider->world_coords = unit->world_coords;
            rider->world_coords.vy -= 10;
            rider->real = unit->real;
            rider->real.vy -= 0xA000;
        }
    }
}
