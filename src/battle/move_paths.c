/* Unit path steps along tile edges, knockback and teleport steps, the free cursor, and camera pan, rotation, zoom, tilt
 * and focus tracking. */

#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/battle_unit_animation.h"
#include "fft/main.h"
#include "fft/main_sound.h"
#include "fft/map.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/sound.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Step handler of the FLOAT_JUMP phases: carry a floating unit to the entry edge of its
 * destination tile, where its horizontal motion stops; once it has come down to the tile's height,
 * start the float step into the matching FLOAT_TO_CENTER phase. */
static void battle_move_update_unit_step_to_destination_tile_entry_edge(battle_unit_t* unit) {
    s32 direction = battle_move_get_direction(unit);

    battle_move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        battle_move_displace_unit_at_destination_tile(unit, direction);
    }
    if (battle_move_has_reached_destination_tile_entry_edge(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        if (unit->world_coords.vy >= (s16)battle_gfx_calculate_world_height_from_world_coords(unit)) {
            unit->velocity.vy = 0;
            battle_move_start_float_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
            unit->step_phase = g_battle_move_float_to_center_phases[direction];
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
static void battle_move_update_unit_step_to_current_tile_exit_edge(battle_unit_t* unit) {
    s32 direction = battle_move_get_direction(unit);

    battle_move_apply_unit_step_velocity(unit);
    if (battle_move_has_reached_current_tile_exit_edge(direction, unit) != 0) {
        battle_move_snap_axis_to_current_tile_exit_edge(direction, unit);
        g_battle_move_current_edge_height = g_battle_move_current_tile->height * 2
            + g_battle_move_current_tile->depth_half_height.half_height * unit->current_edge_height;
        unit->standing_on_battle_id_plus_one = 0;
        g_battle_move_destination_edge_height = g_battle_move_destination_tile->height * 2
            + g_battle_move_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
        battle_move_set_velocity_for_contiguous_steps_with_final_tile_height(
            unit, &unit->path, &unit->current_path_step);
        battle_move_interpolate_partial(unit, unit->walk_speed);
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
static void battle_move_update_unit_vertical_step_to_destination_tile_center(battle_unit_t* unit) {
    s32 done = 0;
    s32 direction = battle_move_get_direction(unit);
    battle_unit_t* mount;

    battle_move_apply_unit_step_velocity(unit);
    if (battle_move_has_reached_destination_tile_center(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        g_battle_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
        if (unit->movement_path_offset == unit->path.count && (unit->path.pending_mount & BATTLE_PENDING_MOUNT_SET)) {
            mount = battle_unit_get_by_battle_id(unit->path.pending_mount & BATTLE_PENDING_MOUNT_ID_MASK);
            if (mount != 0) {
                g_battle_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(mount) - 10;
            }
        }
        if (unit->velocity.vy > 0) {
            if (unit->world_coords.vy >= g_battle_move_target_screen_z) {
                done = 1;
            }
        } else if (unit->world_coords.vy <= g_battle_move_target_screen_z) {
            done = 1;
        }
        if (unit->movement_path_offset != unit->path.count) {
            done = 1;
        }
        if (done) {
            unit->velocity.vy = 0;
            battle_move_snap_axis_to_destination_tile_center(direction, unit);
            unit->velocity.vz = 0;
            unit->velocity.vx = 0;
            g_battle_move_target_screen_z = battle_gfx_calculate_world_height_from_world_coords(unit);
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->shadow_dirty |= 1;
        }
    }
}

/* Selects the slope bits of the two step tiles that face the movement
 * direction (top two bits of g_battle_move_step_value) and stores them as the unit's
 * per-step scales; a rider or a g_battle_move_step_value bit 0x10 step clears them. */
static void battle_move_set_unit_step_slope_scales(battle_unit_t* unit) {
    battle_move_get_current_and_destination_tiles(unit, &g_battle_move_current_tile, &g_battle_move_destination_tile);
    battle_gfx_check_tile_status_palette_mod(unit, g_battle_move_destination_tile->flags.shading, 1);
    switch (g_battle_move_step_value.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = (g_battle_move_destination_tile->slope_type & 0xC) >> 2;
        unit->current_edge_height = g_battle_move_current_edge_height = g_battle_move_current_tile->slope_type & 3;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = g_battle_move_destination_tile->slope_type & 3;
        unit->current_edge_height = g_battle_move_current_edge_height
            = (g_battle_move_current_tile->slope_type & 0xC) >> 2;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = g_battle_move_destination_tile->slope_type >> 6;
        unit->current_edge_height = g_battle_move_current_edge_height
            = (g_battle_move_current_tile->slope_type & 0x30) >> 4;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->destination_edge_height = g_battle_move_destination_edge_height
            = (g_battle_move_destination_tile->slope_type & 0x30) >> 4;
        unit->current_edge_height = g_battle_move_current_edge_height = g_battle_move_current_tile->slope_type >> 6;
        break;
    }
    if (unit->standing_on_battle_id_plus_one != 0) {
        unit->current_edge_height = g_battle_move_current_edge_height = 0;
    }
    if (g_battle_move_step_value.mount) {
        unit->destination_edge_height = g_battle_move_destination_edge_height = 0;
    }
}

/* Step handler of the TELEPORT_ANIMATION phase: once the animation countdown
 * reaches 0, end the path and keep its count in last_path_count. */
static void battle_move_finalize_path_after_animation(battle_unit_t* unit) {
    if (unit->animation.wait == 0) {
        unit->step_phase = MOVE_STEP_PHASE_NONE;
        unit->last_path_count = unit->path.count;
        unit->path.count = 0;
    }
}

/* Step handler of the TELEPORT_START phase. */
static void battle_move_update_teleport_start(battle_unit_t* unit) {
    battle_move_start_teleport_animation(unit);
}

/* Called for the last step of a path: when the unit has a pending mount, set the
 * step's mount bit (0x10), which marks a step that ends on another unit. */
static void battle_move_set_unit_path_flag(battle_unit_t* unit) {
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
 * and otherwise the next path byte starts a step (the path's
 * BATTLE_EFFECTIVE_MOVEMENT_FLY and _FLOAT flags pick the fly and float step
 * starters, and the step's slow bit picks the climb speed).
 */
void battle_move_update_path_step(battle_unit_t* unit) {
    u32 offset;
    u8 count;
    u8 flags;

    battle_move_get_current_and_destination_tiles(unit, &g_battle_move_current_tile, &g_battle_move_destination_tile);
    g_battle_move_step_value = unit->current_path_step;
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_JUMP_START_SOUTH:
    case MOVE_STEP_PHASE_JUMP_START_EAST:
    case MOVE_STEP_PHASE_JUMP_START_NORTH:
    case MOVE_STEP_PHASE_JUMP_START_WEST:
        if (unit->animation.wait == 0) {
            unit->step_phase++;
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
            battle_sound_play_movement_sfx(unit, MAIN_SFX_JUMP);
            battle_unit_dismount_rider(unit);
        }
        break;
    case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
    case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
        battle_move_update_knockback_after_animation(unit);
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
        battle_move_update_airborne_ascent_phase(unit);
        /* fallthrough */
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
        battle_move_finish_unit_step_at_tile_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        battle_move_update_unit_step_to_destination_tile_entry_edge(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        battle_move_update_unit_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        battle_move_update_float_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        battle_move_update_unit_vertical_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        battle_move_update_walking_step_at_tile_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        battle_move_update_float_step_to_current_tile_exit_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        battle_move_update_unit_step_to_current_tile_exit_edge(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_START:
        battle_move_update_teleport_start(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_ANIMATION:
        battle_move_finalize_path_after_animation(unit);
        break;
    }
    if (unit->step_phase == MOVE_STEP_PHASE_NONE) {
        count = unit->path.count;
        if (count == MOVE_PATH_TELEPORT) {
            battle_unit_dismount_rider_and_update_display(unit);
            battle_move_start_teleport_step(unit);
        } else if (count == MOVE_PATH_TELEPORT_FAILED) {
            battle_unit_dismount_rider_and_update_display(unit);
            battle_move_start_teleport_step(unit);
            unit->path.pending_mount = 0;
            battle_ai_init_selected_action();
        } else if (count == 0) {
            battle_unit_mount_onto_pending_mount(unit);
        } else if (unit->movement_path_offset < count) {
            if (unit->movement_path_offset == count - 1) {
                battle_move_set_unit_path_flag(unit);
            }
            offset = unit->movement_path_offset;
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_battle_move_step_value = unit->path.data.steps[offset];
            battle_move_set_unit_step_slope_scales(unit);
            flags = unit->path.flags;
            if (flags & BATTLE_EFFECTIVE_MOVEMENT_FLY) {
                battle_move_start_fly_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
            } else if (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) {
                battle_move_start_float_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
            } else {
                switch (g_battle_move_step_value.slow) {
                case 0:
                    unit->step_speed = 0x2000;
                    battle_move_start_unit_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
                    break;
                case 1:
                    battle_move_start_unit_step_at_climb_speed(
                        unit, g_battle_move_current_tile, g_battle_move_destination_tile);
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
void battle_move_update_knockback_step(battle_unit_t* unit) {
    u32 offset;
    s32 speed;

    battle_move_get_current_and_destination_tiles(unit, &g_battle_move_current_tile, &g_battle_move_destination_tile);
    g_battle_move_step_value = unit->current_path_step;
    if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
        speed = unit->walk_speed;
        unit->walk_speed = (speed > ONE) ? speed / 4 * 3 : ONE;
        switch (unit->step_phase) {
        case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
            battle_move_update_unit_step_to_destination_tile_center(unit);
            break;
        case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
            battle_move_update_walking_step_at_tile_edge(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
        case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
            battle_move_update_knockback_after_animation(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
            battle_move_update_airborne_ascent_phase(unit);
            /* fallthrough */
        case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
            battle_move_finish_unit_step_at_tile_edge(unit);
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
            unit->current_path_step = g_battle_move_step_value = unit->path.data.steps[offset];
            battle_move_set_unit_step_slope_scales(unit);
            unit->step_speed = 0x2000;
            battle_move_start_unit_step(unit, g_battle_move_current_tile, g_battle_move_destination_tile);
        } else {
            battle_unit_settle_on_tile(unit);
        }
    }
}

/* Give a knocked-back unit a one-step path to its destination: a plain step in the travel
 * direction with the destination level as its elevation bit, at walk speed 0x8000 (four times the
 * usual 0x2000), which battle_move_update_knockback_step then slows.
 *
 * The masks edit path.data.steps[0] as bits 8-15 of the path's first word: 0x300 length,
 * 0x400 slow, 0x800 climb, 0x1000 mount, 0x2000 higher and 0xc000 the move_step_direction_e. */
static void battle_move_init_knockback(battle_unit_t* unit) {
    u32* path_word = (u32*)&unit->path;
    u32 state;
    u32 knockback_state;
    s32 direction;

    unit->movement_path_offset = 0;
    unit->path.count = 1;
    unit->path.flags = battle_move_get_effective_flags(unit->stats);

    state = *path_word;
    state &= ~0x300;
    state &= ~0x400;
    state &= ~0x800;
    state &= ~0x1000;
    *path_word = state;

    knockback_state = state & ~0x2000;
    knockback_state |= (unit->movement.destination_z & 1) << 13;
    *path_word = knockback_state;
    direction = battle_move_get_direction(unit);

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
void battle_move_start_knockback(const battle_unit_t* src, battle_unit_t* dst) {
    dst->movement.destination_x = src->strike.target_new_x;
    dst->movement.destination_y = src->strike.target_new_y;
    dst->movement.destination_z = src->strike.target_new_map_level;
    battle_move_init_knockback(dst);
}

/* Map the directional buttons to a map direction (battle facing, diagonals included) for the
 * camera's yaw quadrant and the cursor input mode. source picks held, newly pressed (repeating at
 * the cursor repeat speed) or released buttons; returns -1 for no direction or when a face button,
 * Select or Start was just pressed. */
s32 battle_camera_get_input_direction(s32 source) {
    s32 direction = -1;
    u32 input;
    u32 previous;
    u32 pressed;
    u32 divisor;
    u32 threshold;

    /* Pick the button word that source names for the current cursor input mode. */
    switch (g_battle_cursor_input_mode) {
    case BATTLE_CURSOR_INPUT_OPTION_A:
    case BATTLE_CURSOR_INPUT_OPTION_B:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons;
            break;
        case CAMERA_INPUT_PRESSED:
            input = g_main_input_pressed_buttons;
            switch (g_main_game_options.fields.cursor_repeat_speed) {
            case GAME_CURSOR_REPEAT_SPEED_FAST:
                divisor = 3;
                threshold = 15;
                break;
            case GAME_CURSOR_REPEAT_SPEED_REGULAR:
                divisor = 6;
                threshold = 30;
                break;
            case GAME_CURSOR_REPEAT_SPEED_SLOW:
            default:
                divisor = 12;
                threshold = 60;
                break;
            }
            /* Buttons held past threshold frames count as pressed again every divisor frames. */
            if (threshold < g_main_input_stable_frames && g_main_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_main_input_released_buttons;
            break;
        }
        break;
    case BATTLE_CURSOR_INPUT_FREE_CURSOR:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons | g_main_input_previous_copy_0;
            break;
        case CAMERA_INPUT_PRESSED:
            previous = g_input_previous_buttons | g_main_input_previous_copy_0;
            pressed = g_main_input_pressed_buttons;
            input = 0;

            /* A new press counts only when it completes a diagonal with a held perpendicular button. */
            if (pressed & PSX_PAD_UP) {
                if (previous & PSX_PAD_RIGHT) {
                    input = PSX_PAD_UP | PSX_PAD_RIGHT;
                } else if (previous & PSX_PAD_LEFT) {
                    input = PSX_PAD_UP | PSX_PAD_LEFT;
                }
            } else if (pressed & PSX_PAD_DOWN) {
                if (previous & PSX_PAD_RIGHT) {
                    input = PSX_PAD_RIGHT | PSX_PAD_DOWN;
                } else if (previous & PSX_PAD_LEFT) {
                    input = PSX_PAD_DOWN | PSX_PAD_LEFT;
                }
            } else if (pressed & PSX_PAD_LEFT) {
                if (previous & PSX_PAD_UP) {
                    input = PSX_PAD_UP | PSX_PAD_LEFT;
                } else if (previous & PSX_PAD_DOWN) {
                    input = PSX_PAD_DOWN | PSX_PAD_LEFT;
                }
            } else if (pressed & PSX_PAD_RIGHT) {
                if (previous & PSX_PAD_UP) {
                    input = PSX_PAD_UP | PSX_PAD_RIGHT;
                } else if (previous & PSX_PAD_DOWN) {
                    input = PSX_PAD_RIGHT | PSX_PAD_DOWN;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_UP) {
                /* battle_state_update_controller_input stores last frame's presses in both copy_0 and
                 * copy_1, so these lone-direction tests never pass: a lone direction registers only
                 * through the auto-repeat below. */
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_UP) && (previous & PSX_PAD_UP)) {
                    input = PSX_PAD_UP;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_DOWN) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_DOWN) && (previous & PSX_PAD_DOWN)) {
                    input = PSX_PAD_DOWN;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_LEFT) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_LEFT) && (previous & PSX_PAD_LEFT)) {
                    input = PSX_PAD_LEFT;
                }
            } else if (g_main_input_pressed_copy_1 & PSX_PAD_RIGHT) {
                if (!((pressed | g_main_input_pressed_copy_0) & PSX_PAD_RIGHT) && (previous & PSX_PAD_RIGHT)) {
                    input = PSX_PAD_RIGHT;
                }
            }

            switch (g_main_game_options.fields.cursor_repeat_speed) {
            case GAME_CURSOR_REPEAT_SPEED_FAST:
                divisor = 6;
                threshold = 6;
                break;
            case GAME_CURSOR_REPEAT_SPEED_REGULAR:
                divisor = 10;
                threshold = 15;
                break;
            case GAME_CURSOR_REPEAT_SPEED_SLOW:
            default:
                divisor = 16;
                threshold = 30;
                break;
            }
            if (threshold < g_main_input_stable_frames && g_main_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons | g_main_input_previous_copy_0;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_main_input_released_buttons;
            break;
        }
        break;
    }

    /* Rotate the buttons by the camera's yaw quadrant into a battle facing for the mode's layout. */
    switch (g_battle_cursor_input_mode) {
    case BATTLE_CURSOR_INPUT_OPTION_A:
        switch (g_battle_camera_render_state.vy & 0xC00) {
        case BATTLE_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_EAST;
            }
            break;
        case BATTLE_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH;
            }
            break;
        case BATTLE_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_WEST;
            }
            break;
        case BATTLE_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH;
            }
            break;
        }
        break;
    case BATTLE_CURSOR_INPUT_OPTION_B:
        switch (g_battle_camera_render_state.vy & 0xC00) {
        case BATTLE_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH;
            }
            break;
        case BATTLE_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_WEST;
            }
            break;
        case BATTLE_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH;
            }
            break;
        case BATTLE_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_EAST;
            }
            break;
        }
        break;
    case BATTLE_CURSOR_INPUT_FREE_CURSOR:
        switch (g_battle_camera_render_state.vy & 0xC00) {
        case BATTLE_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_NORTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH_EAST;
            }
            break;
        case BATTLE_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_SOUTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_NORTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_NORTH_WEST;
            }
            break;
        case BATTLE_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_SOUTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH_WEST;
            }
            break;
        case BATTLE_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = BATTLE_FACING_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_NORTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = BATTLE_FACING_SOUTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = BATTLE_FACING_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = BATTLE_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = BATTLE_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = BATTLE_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = BATTLE_FACING_SOUTH_EAST;
            }
            break;
        }
        break;
    }

    if (g_main_input_pressed_buttons
        & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT | PSX_PAD_START)) {
        direction = -1;
    }
    return direction;
}

/* Start the hovered-unit stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void battle_target_store_cursor_unit_name_and_data(void) {
    battle_unit_t* unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    s32 mode;
    s32 unit_id;

    if (unit != 0) {
        mode = UNIT_VIEW_MODE_HOVERED_UNIT;
        unit_id = unit->stats->battle_id;
    } else {
        mode = UNIT_VIEW_MODE_TILE_INFO;
        unit_id = 0xFF;
    }
    menu_start_unit_view_thread(mode, unit_id, 0xFF);
}

/* Start the right-side stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void battle_target_store_cursor_unit_as_preview_target(void) {
    battle_unit_t* unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    battle_unit_get_source();
    if (unit != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_RIGHT_UNIT, 0xFF, unit->stats->battle_id);
    } else {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    }
}

/* Start the unit panels for a selected target tile: the attack preview for the
 * source unit against the unit on the cursor tile, or the source unit's own stats
 * when the tile is empty. */
void battle_target_store_cursor_casting_unit_name_and_data(void) {
    battle_unit_t* target
        = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    battle_unit_t* source = battle_unit_get_source();

    if (target != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_ATTACK_PREVIEW, source->stats->battle_id, target->stats->battle_id);
    } else {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_HOVERED_UNIT, source->stats->battle_id, 0xFF);
    }
}

/* Move the cursor while a target is being chosen: onto selected_unit when one is
 * given, otherwise to the tile stored in the unit's cursor command state.
 *
 * When the cursor tile changes, the unit panels are refreshed for the current
 * game state: the cursor unit's panel in the free-cursor, close-move-help and
 * target-select states, the preview target's panel in the targeting-range state. */
void battle_target_update_free_cursor_selection(const battle_unit_t* unit, const battle_unit_t* selected_unit) {
    s32 old_x = g_battle_cursor_x;
    s32 old_z = g_battle_cursor_z;
    s32 old_y = g_battle_cursor_y;

    if (selected_unit != 0) {
        battle_target_move_cursor_to_unit(selected_unit);
    } else {
        g_battle_cursor_x = unit->command_state.cursor.target_panel.vx;
        g_battle_cursor_z = unit->command_state.cursor.target_panel.vy;
        g_battle_cursor_y = unit->command_state.cursor.target_panel.vz;
    }
    if (((g_battle_cursor_x != old_x) | (g_battle_cursor_z != old_z) | (g_battle_cursor_y != old_y)) != 0) {
        switch (g_battle_game_state) {
        case BATTLE_GAME_STATE_FREE_CURSOR:
        case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
        case BATTLE_GAME_STATE_TARGET_SELECT:
            battle_target_store_cursor_unit_name_and_data();
            break;
        case BATTLE_GAME_STATE_TARGETING_RANGE:
            battle_target_store_cursor_unit_as_preview_target();
            break;
        }
    }
}

/* Moves the free cursor one step from the pad direction and reports whether
 * it moved.
 *
 * Before reading the direction, the cursor may hop to the other map level:
 * on START when the multi-height cursor option is 3, otherwise once the
 * repeat counter passes the option's delay. A step onto a blocked tile whose
 * other level is also blocked is undone; otherwise the cursor takes the
 * unblocked level, or the one nearer the old tile's height.
 *
 * `direction` doubles as the level-hop flag, as the target keeps both in one
 * register. The do-while(0) weights the delay's references so that GCC
 * allocates it before `direction`; without it the two swap s0 and s1. The
 * 32-bit option value is copied first so the loop note does not stop the load from
 * being scheduled into the prologue. */
s32 battle_target_move_cursor_by_input(void) {
    game_options_t options = g_main_game_options;
    u32 delay;
    s32 direction;
    map_tile_t* current;
    map_tile_t* lower;
    map_tile_t* upper;
    map_tile_t* other_level;
    s8 lower_rise;
    s8 upper_rise;

    do {
        switch (options.fields.multi_height_cursor_speed) {
        case 0:
            delay = 30;
            break;
        case 1:
            delay = 60;
            break;
        case 2:
        default:
            delay = 90;
            break;
        }
    } while (0);

    direction = 0;
    current = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (g_main_game_options.fields.multi_height_cursor_speed == 3) {
        if (g_main_input_pressed_buttons & PSX_PAD_START) {
            other_level = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
            if (!other_level->flags.blocked) {
                direction = 1;
                g_battle_cursor_z ^= 1;
            }
        }
    } else if (g_battle_cursor_repeat_counter > delay) {
        g_battle_cursor_repeat_counter = 0;
        other_level = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
        if (!other_level->flags.blocked) {
            direction = 1;
            g_battle_cursor_z ^= 1;
        }
    }

    if (direction == 0) {
        g_battle_cursor_repeat_counter++;
        g_battle_cursor_previous_x = g_battle_cursor_x;
        g_battle_cursor_previous_z = g_battle_cursor_z;
        g_battle_cursor_previous_y = g_battle_cursor_y;

        direction = battle_camera_get_input_direction(CAMERA_INPUT_PRESSED);
        switch (direction) {
        case BATTLE_FACING_NORTH:
            g_battle_cursor_y++;
            break;
        case BATTLE_FACING_SOUTH:
            g_battle_cursor_y--;
            break;
        case BATTLE_FACING_WEST:
            g_battle_cursor_x--;
            break;
        case BATTLE_FACING_EAST:
            g_battle_cursor_x++;
            break;
        case BATTLE_FACING_NORTH_EAST:
            g_battle_cursor_y++;
            g_battle_cursor_x++;
            break;
        case BATTLE_FACING_NORTH_WEST:
            g_battle_cursor_y++;
            g_battle_cursor_x--;
            break;
        case BATTLE_FACING_SOUTH_EAST:
            g_battle_cursor_x++;
            g_battle_cursor_y--;
            break;
        case BATTLE_FACING_SOUTH_WEST:
            g_battle_cursor_y--;
            g_battle_cursor_x--;
            break;
        }

        /* Keep the cursor on the map: a step past the edge goes back, and a step below 0
         * compares as a large unsigned value and resets to 0. */
        if (g_battle_cursor_x == g_battle_map_max_x) {
            g_battle_cursor_x--;
        } else if ((u32)g_battle_cursor_x > g_battle_map_max_x) {
            g_battle_cursor_x = 0;
        }
        if (g_battle_cursor_y == g_battle_map_max_y) {
            g_battle_cursor_y = g_battle_map_max_y - 1;
        }
        if ((u32)g_battle_cursor_y > g_battle_map_max_y) {
            g_battle_cursor_y = 0;
        }

        lower = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        upper = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
        if (lower->flags.blocked) {
            if (upper->flags.blocked) {
                switch (direction) {
                case BATTLE_FACING_NORTH:
                    g_battle_cursor_y--;
                    break;
                case BATTLE_FACING_SOUTH:
                    g_battle_cursor_y++;
                    break;
                case BATTLE_FACING_WEST:
                    g_battle_cursor_x++;
                    break;
                case BATTLE_FACING_EAST:
                    g_battle_cursor_x--;
                    break;
                case BATTLE_FACING_NORTH_EAST:
                    g_battle_cursor_y--;
                    g_battle_cursor_x--;
                    break;
                case BATTLE_FACING_NORTH_WEST:
                    g_battle_cursor_y--;
                    g_battle_cursor_x++;
                    break;
                case BATTLE_FACING_SOUTH_EAST:
                    g_battle_cursor_x--;
                    g_battle_cursor_y++;
                    break;
                case BATTLE_FACING_SOUTH_WEST:
                    g_battle_cursor_y++;
                    g_battle_cursor_x++;
                    break;
                }
            } else {
                g_battle_cursor_z ^= 1;
            }
        } else if (!upper->flags.blocked) {
            lower_rise = lower->height - current->height;
            upper_rise = upper->height - current->height;
            if (lower_rise < 0) {
                lower_rise = -lower_rise;
            }
            if (upper_rise < 0) {
                upper_rise = -upper_rise;
            }
            if (upper_rise < lower_rise) {
                g_battle_cursor_z ^= 1;
            }
        }
    }

    if (((g_battle_cursor_x != g_battle_cursor_previous_x) | (g_battle_cursor_z != g_battle_cursor_previous_z)
            | (g_battle_cursor_y != g_battle_cursor_previous_y))
        != 0) {
        main_sound_play_sfx(MAIN_SFX_CURSOR_MOVE);
        g_battle_cursor_repeat_counter = 0;
        switch (g_battle_game_state) {
        case BATTLE_GAME_STATE_FREE_CURSOR:
        case BATTLE_GAME_STATE_CLOSE_MOVE_HELP:
        case BATTLE_GAME_STATE_TARGET_SELECT:
            battle_target_store_cursor_unit_name_and_data();
            break;
        case BATTLE_GAME_STATE_TARGETING_RANGE:
            battle_target_store_cursor_unit_as_preview_target();
            break;
        }
        return 1;
    }
    return 0;
}

/* Start free map panning: held directions move the camera (battle_camera_update_map_pan) and the
 * camera stops following the cursor tile. */
void battle_camera_enable_map_pan(void) {
    g_battle_camera_map_pan_enabled = 1;
}

/* End free map panning: the camera follows the cursor tile again. */
void battle_camera_disable_map_pan(void) {
    g_battle_camera_map_pan_enabled = 0;
}

/* Pans the camera with the directional buttons, once per frame of the battle
 * loop, while g_battle_camera_map_pan_enabled is set (the states that put the
 * buttons in free-cursor mode: unit highlighting, move-area display and ability
 * preview).
 *
 * Each held direction moves the x or map-depth coordinate by 0x4000 (four world
 * units in 20.12) per frame, diagonals both. The position is then clamped to four
 * tiles (0x70000) outside the map on every side: -0x70000 up to
 * g_battle_map_max_x or g_battle_map_max_y tiles of 0x1c000 plus 0x70000. */
void battle_camera_update_map_pan(void) {
    s32* coord;
    s32 limit;

    if (g_battle_camera_map_pan_enabled != 0) {
        switch (battle_camera_get_input_direction(CAMERA_INPUT_HELD)) {
        case BATTLE_FACING_NORTH:
            coord = &g_battle_camera_current_real_coords.vz;
            *coord += 4 * ONE;
            break;
        case BATTLE_FACING_SOUTH:
            coord = &g_battle_camera_current_real_coords.vz;
            *coord -= 4 * ONE;
            break;
        case BATTLE_FACING_WEST:
            coord = &g_battle_camera_current_real_coords.vx;
            *coord -= 4 * ONE;
            break;
        case BATTLE_FACING_EAST:
            coord = &g_battle_camera_current_real_coords.vx;
            *coord += 4 * ONE;
            break;
        case BATTLE_FACING_NORTH_EAST: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y += 4 * ONE;
            g_battle_camera_current_real_coords.vx += 4 * ONE;
            break;
        }
        case BATTLE_FACING_NORTH_WEST: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y += 4 * ONE;
            g_battle_camera_current_real_coords.vx -= 4 * ONE;
            break;
        }
        case BATTLE_FACING_SOUTH_EAST: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y -= 4 * ONE;
            g_battle_camera_current_real_coords.vx += 4 * ONE;
            break;
        }
        case BATTLE_FACING_SOUTH_WEST: {
            s32* y = &g_battle_camera_current_real_coords.vz;
            *y -= 4 * ONE;
            g_battle_camera_current_real_coords.vx -= 4 * ONE;
            break;
        }
        }
        {
            s32* x = &g_battle_camera_current_real_coords.vx;
            limit = (g_battle_map_max_x * (MAP_TILE_SIZE * ONE)) + 4 * MAP_TILE_SIZE * ONE;
            if (limit < *x)
                *x = limit;
            if (*x < -(4 * MAP_TILE_SIZE * ONE))
                *x = -(4 * MAP_TILE_SIZE * ONE);
        }
        limit = (g_battle_map_max_y * (MAP_TILE_SIZE * ONE)) + 4 * MAP_TILE_SIZE * ONE;
        if (limit < g_battle_camera_current_real_coords.vz)
            g_battle_camera_current_real_coords.vz = limit;
        if (g_battle_camera_current_real_coords.vz < -(4 * MAP_TILE_SIZE * ONE))
            g_battle_camera_current_real_coords.vz = -(4 * MAP_TILE_SIZE * ONE);
    }
}

/* Set the destination of a left turn (falling yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
static void battle_camera_set_left_turn_destination(s32 target_angle) {
    g_battle_previous_camera_yaw = g_battle_camera_render_state.vy & 0xfe00;
    g_battle_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_battle_camera_yaw_target = target_angle + 0x200;
    }
}

/* Start a left turn to target_angle from the slowest speed. */
static void battle_camera_start_rotation_left(s32 target_angle) {
    g_battle_camera_rotation_action = CAMERA_ROTATION_LEFT;
    g_battle_camera_rotation_increment = 1;
    battle_camera_set_left_turn_destination(target_angle);
}

/* Set the destination of a right turn (rising yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
static void battle_camera_set_right_turn_destination(s32 target_angle) {
    g_battle_previous_camera_yaw = g_battle_camera_render_state.vy & 0xfe00;
    g_battle_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_battle_camera_yaw_target = target_angle - 0x200;
    }
}

/* Start a right turn to target_angle from the slowest speed. */
static void battle_camera_start_rotation_right(s32 target_angle) {
    g_battle_camera_rotation_action = CAMERA_ROTATION_RIGHT;
    g_battle_camera_rotation_increment = 1;
    battle_camera_set_right_turn_destination(target_angle);
}

/* Start an automatic turn of angle yaw units (0x400 is a quarter turn;
 * negative turns left) with its sound, unless a rotation is running. */
static void battle_camera_request_rotation(s32 angle) {
    if (g_battle_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (angle < 0) {
            battle_camera_start_rotation_left((g_battle_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_LEFT);
        } else {
            battle_camera_start_rotation_right((g_battle_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}

/* L1 starts a quarter turn left and R1 one right, each with its sound, unless
 * a rotation is running. */
void battle_camera_handle_rotation_input(void) {
    if (g_battle_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (g_main_input_pressed_buttons & PSX_PAD_L1) {
            battle_camera_start_rotation_left((g_battle_camera_render_state.vy & 0xFE00) - 0x400);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_LEFT);
        }
        if (g_main_input_pressed_buttons & PSX_PAD_R1) {
            battle_camera_start_rotation_right((g_battle_camera_render_state.vy & 0xFE00) + 0x400);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}

/* Step the yaw toward its destination by an increment that grows by twice the
 * animation speed each frame until it passes 0x2f. On arrival the yaw snaps to
 * the destination; while L1 (turning left) or R1 (turning right) is still held
 * outside an event, the next quarter turn starts at the speed reached, and
 * otherwise the rotation ends: both rotate sounds stop and the stop sound
 * plays. */
void battle_camera_update_rotation(void) {
    s32 angle;

    if (g_battle_camera_rotation_action == CAMERA_ROTATION_LEFT) {
        s16* yaw = &g_battle_camera_render_state.vy;
        *yaw -= g_battle_camera_rotation_increment;
        if (*yaw <= g_battle_camera_yaw_target) {
            angle = (u16)g_battle_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_L1) {
                if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
                    battle_camera_set_left_turn_destination((angle & 0xFE0) - 0x400);
                }
            } else {
                g_battle_camera_rotation_action = CAMERA_ROTATION_NONE;
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_RIGHT);
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_LEFT);
                main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else if (g_battle_camera_rotation_action == CAMERA_ROTATION_RIGHT) {
        s16* yaw = &g_battle_camera_render_state.vy;
        *yaw += g_battle_camera_rotation_increment;
        if (*yaw >= g_battle_camera_yaw_target) {
            angle = (u16)g_battle_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_R1) {
                if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
                    /* Retail continues the right turn through rotate_left. Harmless: the two
                     * differ only for a destination on an axis, which this one never is. */
                    battle_camera_set_left_turn_destination((angle & 0xFE0) + 0x400);
                }
            } else {
                g_battle_camera_rotation_action = CAMERA_ROTATION_NONE;
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_RIGHT);
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_LEFT);
                main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else {
        return;
    }
    if (g_battle_camera_rotation_increment < 0x30) {
        g_battle_camera_rotation_increment = (g_main_system_vsync_interval * 2) + g_battle_camera_rotation_increment;
    }
}

/* Start a zoom toward ONE (mode 0) or 0xc00 (mode 1). Only zooming in to ONE
 * from below and out to 0xc00 from above pair an action and target that
 * battle_camera_update_zoom steps; the other two cases leave the zoom action
 * set with no progress, which also blocks the L2 toggle. No retail code calls
 * it; the L2 input goes through battle_camera_toggle_zoom. */
void battle_camera_zoom_map(s32 mode) {
    switch (mode) {
    case 0:
        if (g_battle_camera_zoom.vx > ONE) {
            g_battle_map_zoom_target = MAP_ZOOM_IN;
            g_battle_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_battle_camera_zoom.vx < ONE) {
            g_battle_map_zoom_target = MAP_ZOOM_IN;
            g_battle_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    case 1:
        if (g_battle_camera_zoom.vx > 0xC00) {
            g_battle_map_zoom_target = MAP_ZOOM_OUT;
            g_battle_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_battle_camera_zoom.vx < 0xC00) {
            g_battle_map_zoom_target = MAP_ZOOM_OUT;
            g_battle_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_battle_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    }
}

/* Toggle the zoom target between MAP_ZOOM_IN and MAP_ZOOM_OUT, starting the
 * matching zoom with its sound; any other target is left alone. The increment
 * restarts at 4 either way. */
static void battle_camera_toggle_zoom(void) {
    if (g_battle_map_zoom_target == MAP_ZOOM_IN) {
        g_battle_camera_zoom_action = CAMERA_STEP_DECREASE;
        g_battle_map_zoom_target = MAP_ZOOM_OUT;
        main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
    } else if (g_battle_map_zoom_target == MAP_ZOOM_OUT) {
        g_battle_map_zoom_target = MAP_ZOOM_IN;
        g_battle_camera_zoom_action = CAMERA_STEP_INCREASE;
        main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
    }
    g_battle_camera_zoom_increment = 4;
}

/* Toggles the zoom when L2 is newly pressed and no zoom is running. */
void battle_camera_update_zoom_input(void) {
    if (g_battle_camera_zoom_action == CAMERA_STEP_NONE) {
        if ((g_main_input_pressed_buttons & PSX_PAD_L2) != 0) {
            battle_camera_toggle_zoom();
        }
    }
}

/* Step the three zoom scales toward the zoom target, ONE zooming in or 0xc00
 * zooming out, by an increment that grows by four times the animation speed
 * each frame until it reaches 0x300. The zoom ends on the target. */
void battle_camera_update_zoom(void) {
    s32 increment;
    s32* zoom_ptr;

    if (g_battle_camera_zoom_action == CAMERA_STEP_INCREASE && g_battle_map_zoom_target == MAP_ZOOM_IN) {
        s32 zoom;
        s32 below;
        s32 pan_x;
        s32 pan_y;

        zoom_ptr = &g_battle_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_battle_camera_zoom_increment;
        zoom = zoom + increment;
        *zoom_ptr = zoom;
        below = zoom < ONE;
        pan_x = g_battle_camera_zoom.vy;
        pan_y = g_battle_camera_zoom.vz;
        pan_x = pan_x + increment;
        pan_y = pan_y + increment;
        g_battle_camera_zoom.vy = pan_x;
        g_battle_camera_zoom.vz = pan_y;
        if (!below) {
            g_battle_camera_zoom_action = CAMERA_STEP_NONE;
            g_battle_camera_zoom.vz = ONE;
            g_battle_camera_zoom.vy = ONE;
            *zoom_ptr = ONE;
        }
    } else if (g_battle_camera_zoom_action == CAMERA_STEP_DECREASE
        && g_battle_map_zoom_target == g_battle_camera_zoom_action) {
        s32 zoom;
        s32 below;

        zoom_ptr = &g_battle_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_battle_camera_zoom_increment;
        zoom = zoom - increment;
        *zoom_ptr = zoom;
        below = zoom < 0xC01;
        g_battle_camera_zoom.vy -= increment;
        g_battle_camera_zoom.vz -= increment;
        if (below) {
            g_battle_camera_zoom_action = CAMERA_STEP_NONE;
            g_battle_camera_zoom.vz = 0xC00;
            g_battle_camera_zoom.vy = 0xC00;
            *zoom_ptr = 0xC00;
        }
    } else {
        return;
    }
    if (increment < 0x300) {
        g_battle_camera_zoom_increment = g_main_system_vsync_interval * 4 + increment;
    }
}

/* Start a tilt toward the low pitch 0x12e (mode 0) or the high pitch 0x1c0
 * (mode 1) from whichever side the pitch is on. No retail code calls it; the
 * R2 input goes through battle_camera_toggle_tilt. */
void battle_camera_handle_tilt_request(s32 mode) {
    switch (mode) {
    case 0:
        if (g_battle_camera_render_state.vx >= 0x12F) {
            g_battle_map_tilt_target = MAP_TILT_LOW;
            g_battle_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_battle_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_battle_camera_render_state.vx < 0x12E) {
            g_battle_map_tilt_target = MAP_TILT_LOW;
            g_battle_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_battle_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    case 1:
        if (g_battle_camera_render_state.vx >= 0x1C1) {
            g_battle_map_tilt_target = MAP_TILT_HIGH;
            g_battle_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_battle_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_battle_camera_render_state.vx < 0x1C0) {
            g_battle_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_battle_map_tilt_target = MAP_TILT_HIGH;
            g_battle_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    }
}

/* Toggle the tilt target between low and high, starting the tilt at increment 1
 * with its sound. Targets 3 and 4 form a second pair that nothing else sets, so
 * those branches never run. */
static void battle_camera_toggle_tilt(void) {
    s32 target = g_battle_map_tilt_target;

    if (target == MAP_TILT_LOW) {
        g_battle_camera_tilt_action = CAMERA_STEP_INCREASE;
        g_battle_map_tilt_target = MAP_TILT_HIGH;
    } else if (target == MAP_TILT_HIGH) {
        g_battle_map_tilt_target = MAP_TILT_LOW;
        g_battle_camera_tilt_action = CAMERA_STEP_DECREASE;
    } else if (target == 3) {
        g_battle_camera_tilt_action = CAMERA_STEP_DECREASE;
        g_battle_map_tilt_target = 4;
    } else if (target == 4) {
        g_battle_map_tilt_target = MAP_TILT_LOW;
        g_battle_camera_tilt_action = CAMERA_STEP_INCREASE;
    }
    g_battle_camera_tilt_increment = 1;
    main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
}

/* Toggles the tilt when R2 is newly pressed and no tilt is running. */
void battle_camera_update_tilt_input(void) {
    if (g_battle_camera_tilt_action == CAMERA_STEP_NONE) {
        if ((g_main_input_pressed_buttons & PSX_PAD_R2) != 0) {
            battle_camera_toggle_tilt();
        }
    }
}

/* Step the pitch toward the tilt target, 0x1c0 (high) or 0x12e (low), from
 * either side, by an increment that grows by twice the animation speed each
 * frame until it passes 0x2f. The tilt ends on the target. */
void battle_camera_update_tilt(void) {
    s16 tilt;

    if (g_battle_camera_tilt_action == CAMERA_STEP_INCREASE
        && g_battle_map_tilt_target == g_battle_camera_tilt_action) {
        s16* state = &g_battle_camera_render_state.vx;
        tilt = *state + g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x1C0) {
            g_battle_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x1C0;
        }
    } else if (g_battle_camera_tilt_action == CAMERA_STEP_DECREASE && g_battle_map_tilt_target == MAP_TILT_LOW) {
        s16* state = &g_battle_camera_render_state.vx;
        tilt = *state - g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x12F) {
            g_battle_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x12E;
        }
    } else if (g_battle_camera_tilt_action == CAMERA_STEP_DECREASE && g_battle_map_tilt_target == MAP_TILT_HIGH) {
        s16* state = &g_battle_camera_render_state.vx;
        tilt = *state - g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x1C1) {
            g_battle_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x1C0;
        }
    } else if (g_battle_camera_tilt_action == CAMERA_STEP_INCREASE && g_battle_map_tilt_target == MAP_TILT_LOW) {
        s16* state = &g_battle_camera_render_state.vx;
        tilt = *state + g_battle_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x12E) {
            g_battle_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x12E;
        }
    } else {
        return;
    }
    if (g_battle_camera_tilt_increment < 0x30) {
        g_battle_camera_tilt_increment += g_main_system_vsync_interval * 2;
    }
}

/* Aims the camera step vector from the camera position at the acting unit: the
 * unit-length direction (ONE) with its height component multiplied by four. */
static void battle_camera_calculate_acting_unit_vector_normal(void) {
    VECTOR delta;
    battle_unit_t* unit = battle_unit_get_source();

    setVector(&delta, unit->world_coords.vx - g_battle_camera_current_real_coords.vx / ONE,
        unit->world_coords.vy - g_battle_camera_current_real_coords.vy / ONE,
        unit->world_coords.vz - g_battle_camera_current_real_coords.vz / ONE);
    VectorNormal(&delta, &g_battle_camera_step_vector);
    g_battle_camera_step_vector.vy = g_battle_camera_step_vector.vy << 2;
}

/* The acting-unit camera step vector at double length, used when the unit is
 * outside the wider screen window. */
static void battle_camera_shift_acting_unit_vector(void) {
    battle_camera_calculate_acting_unit_vector_normal();
    g_battle_camera_step_vector.vx *= 2;
    g_battle_camera_step_vector.vy *= 2;
    g_battle_camera_step_vector.vz *= 2;
}

/* Aim the camera vector from the current camera focus at the cursor tile.
 *
 * The tile surface height is (height + depth) * 12 + half-height * 6 world
 * units above the floor, negated because world Y points down. The focus
 * offset is normalized by VectorNormal into g_battle_camera_step_vector and
 * tripled. Returns the negated tile height. */
static s32 battle_camera_calculate_cursor_tile_vector_normal(void) {
    map_tile_t* tile = battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    SVECTOR tile_position;
    VECTOR delta;
    s32 height;
    map_tile_depth_t depth;

    battle_map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
    depth = tile->depth_half_height;
    height = -((tile->height + depth.depth) * MAP_HEIGHT_STEP + depth.half_height * (MAP_HEIGHT_STEP / 2));
    main_util_set_svector(&tile_position, g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), height,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    setVector(&delta, tile_position.vx - g_battle_camera_current_real_coords.vx / ONE,
        height - g_battle_camera_current_real_coords.vy / ONE,
        tile_position.vz - g_battle_camera_current_real_coords.vz / ONE);
    VectorNormal(&delta, &g_battle_camera_step_vector);
    g_battle_camera_step_vector.vx *= 3;
    g_battle_camera_step_vector.vy *= 3;
    g_battle_camera_step_vector.vz *= 3;
    return height;
}

/* Doubles the cursor tile vector computed by
 * battle_camera_calculate_cursor_tile_vector_normal and passes that routine's
 * result through; callers hand it to battle_camera_step_focus_toward_cursor_tile. */
static s32 battle_camera_scale_cursor_tile_vector(void) {
    s32 result = battle_camera_calculate_cursor_tile_vector_normal();

    g_battle_camera_step_vector.vx *= 2;
    g_battle_camera_step_vector.vy *= 2;
    g_battle_camera_step_vector.vz *= 2;
    return result;
}

/* Move one 20.12 fixed-point camera component toward its bound by its per-frame
 * step, without overshooting. The bound is compared against the component's
 * whole-unit part, so the clamp lands exactly on bound * ONE. The test is
 * written once per sign of the step because the overshoot comparison reverses
 * with it. The body is a bare if/else: wrapped in do/while (0) it becomes a
 * loop to the compiler, which doubles its reference counts and changes the
 * register allocation. */
#define CAMERA_STEP_TOWARD(component, step, limit)                                                                     \
    if (step < 0) {                                                                                                    \
        s32* position = component;                                                                                     \
        s32 whole = *position / ONE;                                                                                   \
        s32 bound = limit;                                                                                             \
        if (whole > bound) {                                                                                           \
            *position += step;                                                                                         \
            if (*position / ONE < bound) {                                                                             \
                *position = bound * ONE;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    } else {                                                                                                           \
        s32* position = component;                                                                                     \
        s32 whole = *position / ONE;                                                                                   \
        s32 bound = limit;                                                                                             \
        if (whole < bound) {                                                                                           \
            *position += step;                                                                                         \
            if (bound < *position / ONE) {                                                                             \
                *position = bound * ONE;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    }

/*
 * Advance the camera focus toward the cursor tile by one frame's step.
 *
 * The horizontal bounds are the center of the cursor tile (28 units per tile,
 * half a tile of bias); the caller supplies the height bound directly.
 */
static void battle_camera_step_focus_toward_cursor_tile(s16 height) {
    CAMERA_STEP_TOWARD(&g_battle_camera_current_real_coords.vx, g_battle_camera_step_vector.vx,
        g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    CAMERA_STEP_TOWARD(&g_battle_camera_current_real_coords.vy, g_battle_camera_step_vector.vy, height);
    CAMERA_STEP_TOWARD(&g_battle_camera_current_real_coords.vz, g_battle_camera_step_vector.vz,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
}

/* Moves the camera position by this frame's step vector (g_battle_camera_step_vector)
 * toward the unit's position without passing it.
 *
 * Each component of g_battle_camera_current_real_coords (20.12) advances only
 * while it is still short of the target in the step's direction, and is set to
 * the target exactly when the step carries it past. Called for the acting unit
 * while it moves, chooses its wait direction or starts an attack animation. */
static void battle_camera_step_real_coords_toward_target(const battle_unit_t* target) {
    if (g_battle_camera_step_vector.vx < 0) {
        if (g_battle_camera_current_real_coords.vx / ONE > target->world_coords.vx) {
            g_battle_camera_current_real_coords.vx += g_battle_camera_step_vector.vx;
            if (g_battle_camera_current_real_coords.vx / ONE < target->world_coords.vx) {
                g_battle_camera_current_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    } else {
        if (g_battle_camera_current_real_coords.vx / ONE < target->world_coords.vx) {
            g_battle_camera_current_real_coords.vx += g_battle_camera_step_vector.vx;
            if (g_battle_camera_current_real_coords.vx / ONE > target->world_coords.vx) {
                g_battle_camera_current_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    }
    if (g_battle_camera_step_vector.vy < 0) {
        if (g_battle_camera_current_real_coords.vy / ONE > target->world_coords.vy) {
            g_battle_camera_current_real_coords.vy += g_battle_camera_step_vector.vy;
            if (g_battle_camera_current_real_coords.vy / ONE < target->world_coords.vy) {
                g_battle_camera_current_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    } else {
        if (g_battle_camera_current_real_coords.vy / ONE < target->world_coords.vy) {
            g_battle_camera_current_real_coords.vy += g_battle_camera_step_vector.vy;
            if (g_battle_camera_current_real_coords.vy / ONE > target->world_coords.vy) {
                g_battle_camera_current_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    }
    if (g_battle_camera_step_vector.vz < 0) {
        if (g_battle_camera_current_real_coords.vz / ONE > target->world_coords.vz) {
            g_battle_camera_current_real_coords.vz += g_battle_camera_step_vector.vz;
            if (g_battle_camera_current_real_coords.vz / ONE < target->world_coords.vz) {
                g_battle_camera_current_real_coords.vz = target->world_coords.vz * ONE;
            }
        }
    } else {
        if (g_battle_camera_current_real_coords.vz / ONE < target->world_coords.vz) {
            /* Retail quirk: only this positive map-depth step doubles the component. */
            g_battle_camera_current_real_coords.vz += g_battle_camera_step_vector.vz * 2;
            if (g_battle_camera_current_real_coords.vz / ONE > target->world_coords.vz) {
                g_battle_camera_current_real_coords.vz = target->world_coords.vz * ONE;
            }
        }
    }
}

/* Per-frame camera vector update, dispatched on the game state.
 *
 * The crystal-learn state tracks the map cursor with a narrow acceptance
 * window; the acting-unit states (unit moving, wait direction and pre-attack
 * animation) track the source unit's camera-relative position and step the
 * real coordinates toward the unit; the other states below
 * BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT take the idle path of
 * battle_camera_update_cursor_tile_vector, and that state and those above it
 * clear the vector.
 *
 * The scaling routine passes the normal routine's result through, and these
 * callers hand it on.
 *
 * The idle path is battle_camera_update_cursor_tile_vector verbatim. The four
 * main_util_set_vector calls are one tail-merged block in the target, placed
 * last so it falls into the epilogue, so the idle path's window test is
 * spelled inverted with the clear-vector call as the function's last
 * statement. Written the natural way, the merged block lands mid-function
 * with an extra `j`. */
void battle_camera_update_vector_from_game_state(void) {
    battle_unit_t* unit;
    s32 height;

    if (g_battle_game_state == BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy
                / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xF9 && g_battle_cursor_screen_x < 0x108
            && g_battle_cursor_screen_y - height >= 0xA1 && g_battle_cursor_screen_y < 0xB0) {
            main_util_set_vector(&g_battle_camera_step_vector, 0, 0, 0);
            return;
        }
        /* The scaling routine passes through the normal routine's height bound. */
        battle_camera_step_focus_toward_cursor_tile(battle_camera_scale_cursor_tile_vector());
        return;
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_UNIT_MOVING || g_battle_game_state == BATTLE_GAME_STATE_WAIT_DIRECTION
        || g_battle_game_state == BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION) {
        unit = battle_unit_get_source();
        height
            = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy / ONE;
        if (unit->camera_relative_x >= 0xB1 && unit->camera_relative_x < 0x150
            && unit->camera_relative_y - height >= 0x51 && unit->camera_relative_y < 0xA0) {
            main_util_set_vector(&g_battle_camera_step_vector, 0, 0, 0);
            return;
        }
        if (unit->camera_relative_x >= 0xA1 && unit->camera_relative_x < 0x160
            && unit->camera_relative_y - height >= 0x41 && unit->camera_relative_y < 0xB0) {
            battle_camera_calculate_acting_unit_vector_normal();
        } else {
            battle_camera_shift_acting_unit_vector();
        }
        battle_camera_step_real_coords_toward_target(unit);
        return;
    }
    if (g_battle_game_state >= BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT) {
        main_util_set_vector(&g_battle_camera_step_vector, 0, 0, 0);
        return;
    }
    if (g_battle_camera_zoom_action == CAMERA_STEP_NONE && g_battle_camera_tilt_action == CAMERA_STEP_NONE
        && g_battle_camera_rotation_action == CAMERA_ROTATION_NONE && g_battle_camera_map_pan_enabled == 0) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy
                / ONE;
        }
        if (g_battle_cursor_screen_x < 0xB1 || g_battle_cursor_screen_x >= 0x150
            || g_battle_cursor_screen_y - height < 0x51 || g_battle_cursor_screen_y >= 0xA0) {
            if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
                && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
                battle_camera_step_focus_toward_cursor_tile(battle_camera_calculate_cursor_tile_vector_normal());
            } else {
                /* The scaling routine passes the normal routine's result through. */
                battle_camera_step_focus_toward_cursor_tile(battle_camera_scale_cursor_tile_vector());
            }
            return;
        }
        main_util_set_vector(&g_battle_camera_step_vector, 0, 0, 0);
    }
}

/* Refresh the cursor-tile camera vector while the map camera is idle.
 *
 * The cursor unit's sprite height (scaled by g_battle_camera_zoom.vy, 4.12 fixed point)
 * offsets the cursor's screen position (g_battle_cursor_screen_x, g_battle_cursor_screen_y). Inside the
 * central window the vector is cleared; nearer the edge the unit tile normal
 * is used as is, otherwise doubled. The tile height left in $v0 by either
 * vector routine is passed on to battle_camera_step_focus_toward_cursor_tile. */
void battle_camera_update_cursor_tile_vector(void) {
    battle_unit_t* unit;
    s32 height;

    if (g_battle_camera_zoom_action == CAMERA_STEP_NONE && g_battle_camera_tilt_action == CAMERA_STEP_NONE
        && g_battle_camera_rotation_action == CAMERA_ROTATION_NONE && g_battle_camera_map_pan_enabled == 0) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_battle_camera_zoom.vy
                / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xB1 && g_battle_cursor_screen_x < 0x150
            && g_battle_cursor_screen_y - height >= 0x51 && g_battle_cursor_screen_y < 0xA0) {
            main_util_set_vector(&g_battle_camera_step_vector, 0, 0, 0);
            return;
        }
        if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
            && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
            battle_camera_step_focus_toward_cursor_tile(battle_camera_calculate_cursor_tile_vector_normal());
        } else {
            /* The scaling routine passes through the normal routine's height bound. */
            battle_camera_step_focus_toward_cursor_tile(battle_camera_scale_cursor_tile_vector());
        }
    }
}

/* Advance the movement of each runtime unit that has a path. Runs every frame of event mode and of
 * the change-map jump-in state. */
void battle_move_update_all_unit_paths(void) {
    s32 i;
    battle_unit_t* unit;

    for (i = 0; i < BATTLE_UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = battle_unit_get_by_runtime_id(i);
        if (unit != 0 && unit->path.count != 0) {
            battle_move_update_path_step(unit);
        }
    }
}

/* Turns the camera when the unit's tile is blocked from the current view.
 *
 * The view quadrant is (-yaw & 0xfff) / 0x400; quadrants 0..3 test the tile's
 * north-west, south-west, south-east and north-east camera-block bits. The
 * steep-pitch nibble of camera_block_masks is used at a pitch of
 * CAMERA_PITCH_STEEP_MASK_THRESHOLD or more, the flat-pitch nibble below
 * it. When the current quadrant's bit is set, the first unblocked of a quarter
 * turn (0x400), the opposite quarter turn (-0x400) and a half turn (0x800) is
 * requested; with all four bits set the camera stays.
 *
 * Called when the acting unit starts a cast, when the target-display state shows
 * the casting unit's auto-battle target and when the action menus open for a
 * scripted turn. The `direction < 0` adjustment cannot run: the value is already
 * masked to 0..0xfff. */
void battle_camera_rotate_when_unit_tile_not_visible(const battle_unit_t* unit) {
    s32 direction;
    u8 flags;
    map_tile_t* tile = battle_map_get_tile_data_pointer(unit->map_x, unit->map_y, unit->map_z);

    direction = (0 - g_battle_camera_render_state.vy) & 0xFFF;
    if (direction < 0) {
        direction += 0x3FF;
    }
    direction >>= 0xA;
    if (g_battle_camera_render_state.vx >= CAMERA_PITCH_STEEP_MASK_THRESHOLD) {
        flags = tile->camera_block_masks >> MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT;
    } else {
        flags = tile->camera_block_masks & MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    }
    switch (direction) {
    case 0:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 1:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 2:
        if (flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    case 3:
        if (flags & MAP_TILE_CAMERA_BLOCK_NORTHEAST) {
            if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                battle_camera_request_rotation(0x400);
            } else {
                if (!(flags & MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    battle_camera_request_rotation(-0x400);
                } else if (!(flags & MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    battle_camera_request_rotation(0x800);
                }
            }
        }
        break;
    }
}
