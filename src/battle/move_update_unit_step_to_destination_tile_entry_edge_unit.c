#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

void move_update_unit_step_to_destination_tile_entry_edge(battle_unit_t* unit) {
    s32 direction;

    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        move_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_has_reached_destination_tile_entry_edge(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        if (unit->world_coords.vy >= (s16)battle_gfx_calculate_world_height_from_world_coords(unit)) {
            unit->velocity.vy = 0;
            move_start_float_step(unit, g_move_current_tile, g_move_destination_tile);
            unit->step_phase = g_move_float_to_center_phases[direction];
        }
    }
}

/* Step handler of the FLY_TO_EDGE stages: advance a flying unit one frame toward
 * the exit edge of its tile.
 *
 * At the edge it stores the edge heights of the current and destination tiles
 * (height * 2 + slope half-height * edge value), clears the unit it stood on,
 * builds the velocity for the run of contiguous steps and enters the
 * FLY_TO_CENTER stage. */
void move_update_unit_step_to_current_tile_exit_edge(battle_unit_t* unit) {
    s32 direction;

    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (move_has_reached_current_tile_exit_edge(direction, unit) != 0) {
        move_snap_axis_to_current_tile_exit_edge(direction, unit);
        g_move_current_edge_height = g_move_current_tile->height * 2
            + g_move_current_tile->depth_half_height.half_height * unit->current_edge_height;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_destination_edge_height = g_move_destination_tile->height * 2
            + g_move_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
        move_set_velocity_for_contiguous_steps_with_final_tile_height(unit, &unit->path, &unit->current_path_step);
        move_interpolate_partial(unit, unit->walk_speed);
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH;
    }
}

/*
 * Advance a unit's final vertical step onto its destination tile center.
 *
 * Once the tile center is reached, the target height is the tile's screen Z,
 * or 10 above the mount's when the path is complete and pending_mount bit 0x80
 * names a mount. The step ends when the unit reaches that height in its
 * direction of vertical travel (velocity.vy) or when path steps remain; it
 * then snaps to the center, clears velocity, adopts the destination map
 * coordinates and sets shadow_dirty bit 0.
 */
void move_update_unit_vertical_step_to_destination_tile_center(battle_unit_t* unit) {
    s32 done;
    s32 direction;
    battle_unit_t* mount;

    done = 0;
    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (move_has_reached_destination_tile_center(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        g_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
        if (unit->movement_path_offset == unit->path.count && (unit->path.pending_mount & BATTLE_PENDING_MOUNT_SET)) {
            mount = battle_unit_get_by_battle_id(unit->path.pending_mount & BATTLE_PENDING_MOUNT_ID_MASK);
            if (mount != 0) {
                g_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(mount) - 10;
            }
        }
        if (unit->velocity.vy > 0) {
            if (unit->world_coords.vy >= g_move_target_screen_z) {
                done = 1;
            }
        } else if (unit->world_coords.vy <= g_move_target_screen_z) {
            done = 1;
        }
        if (unit->movement_path_offset != unit->path.count) {
            done = 1;
        }
        if (done) {
            unit->velocity.vy = 0;
            move_snap_axis_to_destination_tile_center(direction, unit);
            unit->velocity.vz = 0;
            unit->velocity.vx = 0;
            g_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->shadow_dirty |= 1;
        }
    }
}

/* Selects the slope bits of the two step tiles that face the movement
 * direction (top two bits of g_move_step_value) and stores them as the unit's
 * per-step scales; a rider or a g_move_step_value bit 0x10 step clears them. */
void move_set_unit_step_slope_scales(battle_unit_t* unit) {
    move_get_current_and_destination_tiles(unit, &g_move_current_tile, &g_move_destination_tile);
    battle_gfx_check_tile_status_palette_mod(unit, g_move_destination_tile->flags.shading, 1);
    switch (g_move_step_value.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->destination_edge_height = g_move_destination_edge_height
            = (g_move_destination_tile->slope_type & 0xC) >> 2;
        unit->current_edge_height = g_move_current_edge_height = g_move_current_tile->slope_type & 3;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->destination_edge_height = g_move_destination_edge_height = g_move_destination_tile->slope_type & 3;
        unit->current_edge_height = g_move_current_edge_height = (g_move_current_tile->slope_type & 0xC) >> 2;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->destination_edge_height = g_move_destination_edge_height = g_move_destination_tile->slope_type >> 6;
        unit->current_edge_height = g_move_current_edge_height = (g_move_current_tile->slope_type & 0x30) >> 4;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->destination_edge_height = g_move_destination_edge_height
            = (g_move_destination_tile->slope_type & 0x30) >> 4;
        unit->current_edge_height = g_move_current_edge_height = g_move_current_tile->slope_type >> 6;
        break;
    }
    if (unit->standing_on_battle_id_plus_one != 0) {
        unit->current_edge_height = g_move_current_edge_height = 0;
    }
    if (g_move_step_value.mount) {
        unit->destination_edge_height = g_move_destination_edge_height = 0;
    }
}

/* Step handler of the TELEPORT_ANIMATION phase: once the animation countdown
 * reaches 0, end the path and keep its count in last_path_count. */
void move_finalize_path_after_animation(battle_unit_t* unit) {
    if (unit->animation.wait == 0) {
        unit->step_phase = MOVE_STEP_PHASE_NONE;
        unit->last_path_count = unit->path.count;
        unit->path.count = 0;
    }
}

/* Step handler of the TELEPORT_START phase. */
void move_update_teleport_start(battle_unit_t* unit) {
    move_start_teleport_animation(unit);
}

/* Called for the last step of a path: when the unit has a pending mount, set the
 * step's mount bit (0x10), which marks a step that ends on another unit. */
void move_set_unit_path_flag(battle_unit_t* unit) {
    if (unit->path.pending_mount == 0) {
        return;
    }
    unit->path.data.steps[unit->movement_path_offset].mount = 1;
}

/*
 * Advance a walking unit by one frame: the elaborate sibling of
 * move_update_knockback_step, with the full 0x3c-entry
 * step-phase table instead of the knockback subset.
 *
 * The jump-start phases dismount any rider once the jump animation has run
 * out; the two teleport phases play the teleport. When no step is active the
 * path count selects what follows: MOVE_PATH_TELEPORT starts the
 * teleport, MOVE_PATH_TELEPORT_FAILED also clears the pending mount and
 * restarts the AI action, 0 or an exhausted path mounts the pending mount,
 * and otherwise the next path byte starts a step (flag 0x80 and 0x40 of
 * movement_flags pick their own starters, and bit 2 of the step byte picks the
 * climb speed).
 */
void move_update_path_step(battle_unit_t* unit) {
    u32 offset;
    u8 count;
    u8 flags;

    move_get_current_and_destination_tiles(unit, &g_move_current_tile, &g_move_destination_tile);
    g_move_step_value = unit->current_path_step;
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_JUMP_START_SOUTH:
    case MOVE_STEP_PHASE_JUMP_START_EAST:
    case MOVE_STEP_PHASE_JUMP_START_NORTH:
    case MOVE_STEP_PHASE_JUMP_START_WEST:
        if (unit->animation.wait == 0) {
            unit->step_phase++;
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
            sound_play_movement_sfx(unit, MAIN_SFX_JUMP);
            battle_unit_dismount_rider(unit);
        }
        break;
    case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
    case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
        move_update_knockback_after_animation(unit);
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
        move_update_airborne_ascent_phase(unit);
        /* fallthrough */
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
        move_finish_unit_step_at_tile_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        move_update_unit_step_to_destination_tile_entry_edge(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        move_update_unit_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        move_update_float_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        move_update_unit_vertical_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        move_update_walking_step_at_tile_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        move_update_float_step_to_current_tile_exit_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        move_update_unit_step_to_current_tile_exit_edge(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_START:
        move_update_teleport_start(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_ANIMATION:
        move_finalize_path_after_animation(unit);
        break;
    }
    if (unit->step_phase == MOVE_STEP_PHASE_NONE) {
        count = unit->path.count;
        if (count == MOVE_PATH_TELEPORT) {
            battle_unit_dismount_rider_and_update_display(unit);
            move_start_teleport_step(unit);
        } else if (count == MOVE_PATH_TELEPORT_FAILED) {
            battle_unit_dismount_rider_and_update_display(unit);
            move_start_teleport_step(unit);
            unit->path.pending_mount = 0;
            ai_init_selected_action();
        } else if (count == 0) {
            battle_unit_mount_onto_pending_mount(unit);
        } else if (unit->movement_path_offset < count) {
            if (unit->movement_path_offset == count - 1) {
                move_set_unit_path_flag(unit);
            }
            offset = unit->movement_path_offset;
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_move_step_value = unit->path.data.steps[offset];
            move_set_unit_step_slope_scales(unit);
            flags = unit->path.flags;
            if (flags & BATTLE_EFFECTIVE_MOVEMENT_FLY) {
                move_start_fly_step(unit, g_move_current_tile, g_move_destination_tile);
            } else if (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) {
                move_start_float_step(unit, g_move_current_tile, g_move_destination_tile);
            } else {
                switch (g_move_step_value.slow) {
                case 0:
                    unit->step_speed = 0x2000;
                    move_start_unit_step(unit, g_move_current_tile, g_move_destination_tile);
                    break;
                case 1:
                    move_start_unit_step_at_climb_speed(unit, g_move_current_tile, g_move_destination_tile);
                    break;
                }
            }
        } else {
            battle_unit_mount_onto_pending_mount(unit);
        }
    }
}

/*
 * Advance a knocked-back or path-moving unit by one frame.
 *
 * While a step is in progress (step_phase non-zero) the walk speed
 * decays by a quarter towards 0x1000 and the step phase handler runs. Once
 * no step is active, the next path byte starts a new step, or the mount
 * animation coordinates are stored when the path is exhausted.
 */
void move_update_knockback_step(battle_unit_t* unit) {
    u32 offset;
    s32 speed;

    move_get_current_and_destination_tiles(unit, &g_move_current_tile, &g_move_destination_tile);
    g_move_step_value = unit->current_path_step;
    if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
        speed = unit->walk_speed;
        unit->walk_speed = (speed > ONE) ? speed / 4 * 3 : ONE;
        switch (unit->step_phase) {
        case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
            move_update_unit_step_to_destination_tile_center(unit);
            break;
        case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
            move_update_walking_step_at_tile_edge(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
        case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
            move_update_knockback_after_animation(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
            move_update_airborne_ascent_phase(unit);
            /* fallthrough */
        case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
            move_finish_unit_step_at_tile_edge(unit);
            break;
        }
        if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
            return;
        }
    }
    if (unit->path.count != 0) {
        offset = unit->movement_path_offset;
        if (offset < unit->path.count) {
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_move_step_value = unit->path.data.steps[offset];
            move_set_unit_step_slope_scales(unit);
            unit->step_speed = 0x2000;
            move_start_unit_step(unit, g_move_current_tile, g_move_destination_tile);
        } else {
            battle_unit_store_coordinate_mount_animation_data(unit);
        }
    }
}

void move_init_knockback(battle_unit_t* unit) {
    u32* path_word = (u32*)&unit->path;
    u32 state;
    u32 knockback_state;
    s32 direction;

    unit->movement_path_offset = 0;
    unit->path.count = 1;
    unit->path.flags = move_get_effective_flags(unit->stats);

    state = *path_word;
    state &= ~0x300;
    state &= ~0x400;
    state &= ~0x800;
    state &= ~0x1000;
    *path_word = state;

    knockback_state = state & ~0x2000;
    knockback_state |= (unit->movement.destination_z & 1) << 13;
    *path_word = knockback_state;
    direction = move_get_direction(unit);

    switch (direction) {
    case MOVE_DIRECTION_NORTH:
        *path_word |= 0xc000;
        break;
    case MOVE_DIRECTION_SOUTH:
        *path_word = (*path_word & ~0xc000) | 0x8000;
        break;
    case MOVE_DIRECTION_WEST:
        *path_word = (*path_word & ~0xc000) | 0x4000;
        break;
    case MOVE_DIRECTION_EAST:
        *path_word &= ~0xc000;
        break;
    }

    unit->walk_speed = 0x8000;
}

/* Knockback: copy the destination tile that the attacker's record (src) holds for
 * its target into the target's (dst) movement destination and start the target's
 * one-step knockback path. */
void move_transfer_target_coordinates(const battle_unit_t* src, battle_unit_t* dst) {
    dst->movement.destination_x = src->strike.target_new_x;
    dst->movement.destination_y = src->strike.target_new_y;
    dst->movement.destination_z = src->strike.target_new_map_level;
    move_init_knockback(dst);
}
