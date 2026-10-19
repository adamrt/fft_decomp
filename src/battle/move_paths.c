/* Unit path steps along tile edges, knockback and teleport steps, the free cursor, and camera pan, rotation, zoom, tilt
 * and focus tracking. */

#include "fft/action.h"
#include "fft/ai_decision.h"
#include "fft/animation.h"
#include "fft/battle_map.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/input.h"
#include "fft/move.h"
#include "fft/move_step.h"
#include "fft/option.h"
#include "fft/sound.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/state.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "fft/vector.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* The selected tile before a cursor step, in map column/layer/row order. */
typedef struct cursor_tile_position {
    s32 x;
    s32 layer;
    s32 y;
} cursor_tile_position_t;
typedef char cursor_tile_position_size_must_be_12[sizeof(cursor_tile_position_t) == 12 ? 1 : -1];

/* Camera step rates and cursor repeat state. */
extern s32 g_camera_rotation_increment;
extern s32 g_camera_yaw_target;

/* Camera yaw rounded down to 0x200 (yaw & 0xfe00), stored as each turn's destination is set;
 * nothing reads it. */
extern s32 g_camera_previous_yaw;
extern s32 g_camera_tilt_increment;
extern s32 g_camera_zoom_increment;
extern u32 g_cursor_repeat_counter; /* free-cursor repeat counter */

static cursor_tile_position_t g_cursor_previous_tile = { 0 };

/* Step handler of the FLOAT_JUMP phases: carry a floating unit to the entry edge of its
 * destination tile, where its horizontal motion stops; once it has come down to the tile's height,
 * start the float step into the matching FLOAT_TO_CENTER phase. */
static void move_step_update_float_jump(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);

    move_step_apply_velocity(unit);
    if (g_game_state != GAME_STATE_EVENT) {
        move_step_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_step_has_reached_entry_edge(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        if (unit->world_coords.vy >= (s16)unit_height_calculate_from_world_coords(unit)) {
            unit->velocity.vy = 0;
            move_step_start_float(unit, g_move_step_current_tile, g_move_step_destination_tile);
            unit->step_phase = g_move_step_float_to_center_phases[direction];
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
static void move_step_update_fly_to_edge(unit_t* unit) {
    s32 direction = move_step_get_direction(unit);

    move_step_apply_velocity(unit);
    if (move_step_has_reached_exit_edge(direction, unit) != 0) {
        move_step_snap_axis_to_exit_edge(direction, unit);
        g_move_step_current_edge_height = g_move_step_current_tile->height * 2
            + g_move_step_current_tile->depth_half_height.half_height * unit->current_edge_height;
        unit->standing_on_battle_id_plus_one = 0;
        g_move_step_destination_edge_height = g_move_step_destination_tile->height * 2
            + g_move_step_destination_tile->depth_half_height.half_height * unit->destination_edge_height;
        move_step_set_velocity_with_final_height(unit, &unit->path, &unit->current_path_step);
        move_step_scale_velocity(unit, unit->walk_speed);
        unit->step_phase = MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH;
    }
}

/*
 * Step handler of the FLY_TO_CENTER stages: advance a flying unit onto its destination tile center.
 *
 * Once the tile center is reached, the target height is the tile's screen Z,
 * or 10 above the mount's when the path is complete and pending_mount bit 0x80
 * names a mount. The step ends when the unit reaches that height in its
 * direction of vertical travel (velocity.vy) or when path steps remain; it
 * then snaps to the center, clears velocity, adopts the destination map
 * coordinates and sets shadow_dirty bit 0.
 */
static void move_step_update_fly_to_center(unit_t* unit) {
    s32 done = 0;
    s32 direction = move_step_get_direction(unit);
    unit_t* mount;

    move_step_apply_velocity(unit);
    if (move_step_has_reached_center(direction, unit) != 0) {
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        g_move_step_target_world_y = unit_height_calculate_from_world_coords(unit);
        if (unit->movement_path_offset == unit->path.count && (unit->path.pending_mount & MOVE_PENDING_MOUNT_SET)) {
            mount = unit_get_by_battle_id(unit->path.pending_mount & MOVE_PENDING_MOUNT_ID_MASK);
            if (mount != 0) {
                g_move_step_target_world_y = unit_height_calculate_from_world_coords(mount) - 10;
            }
        }
        if (unit->velocity.vy > 0) {
            if (unit->world_coords.vy >= g_move_step_target_world_y) {
                done = 1;
            }
        } else if (unit->world_coords.vy <= g_move_step_target_world_y) {
            done = 1;
        }
        if (unit->movement_path_offset != unit->path.count) {
            done = 1;
        }
        if (done) {
            unit->velocity.vy = 0;
            move_step_snap_axis_to_center(direction, unit);
            unit->velocity.vz = 0;
            unit->velocity.vx = 0;
            g_move_step_target_world_y = unit_height_calculate_from_world_coords(unit);
            unit->step_phase = MOVE_STEP_PHASE_NONE;
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->shadow_dirty |= 1;
        }
    }
}

/* Store the edge heights the unit's next step crosses: the side of its current
 * tile it leaves and the side of the destination tile it enters, each a 2-bit
 * field of slope_type (bits 0-1 east, 2-3 west, 4-5 south, 6-7 north; 0 low,
 * 1 sloping, 2 high). Both go to the unit's per-step scales and to
 * g_move_step_current_edge_height and g_move_step_destination_edge_height.
 * A unit standing on another unit leaves at edge height 0, and a step onto a
 * unit (the step's mount bit) enters at 0. The step tiles are reloaded first
 * and the unit's palette follows the destination tile's shading. */
static void move_step_set_slope_scales(unit_t* unit) {
    move_step_get_current_and_destination_tiles(unit, &g_move_step_current_tile, &g_move_step_destination_tile);
    sprite_palette_check_tile_status_mod(unit, g_move_step_destination_tile->flags.shading, 1);
    switch (g_move_step_value.direction) {
    case MOVE_STEP_DIRECTION_EAST:
        unit->destination_edge_height = g_move_step_destination_edge_height
            = (g_move_step_destination_tile->slope_type & 0xC) >> 2;
        unit->current_edge_height = g_move_step_current_edge_height = g_move_step_current_tile->slope_type & 3;
        break;
    case MOVE_STEP_DIRECTION_WEST:
        unit->destination_edge_height = g_move_step_destination_edge_height
            = g_move_step_destination_tile->slope_type & 3;
        unit->current_edge_height = g_move_step_current_edge_height = (g_move_step_current_tile->slope_type & 0xC) >> 2;
        break;
    case MOVE_STEP_DIRECTION_SOUTH:
        unit->destination_edge_height = g_move_step_destination_edge_height
            = g_move_step_destination_tile->slope_type >> 6;
        unit->current_edge_height = g_move_step_current_edge_height
            = (g_move_step_current_tile->slope_type & 0x30) >> 4;
        break;
    case MOVE_STEP_DIRECTION_NORTH:
        unit->destination_edge_height = g_move_step_destination_edge_height
            = (g_move_step_destination_tile->slope_type & 0x30) >> 4;
        unit->current_edge_height = g_move_step_current_edge_height = g_move_step_current_tile->slope_type >> 6;
        break;
    }
    if (unit->standing_on_battle_id_plus_one != 0) {
        unit->current_edge_height = g_move_step_current_edge_height = 0;
    }
    if (g_move_step_value.mount) {
        unit->destination_edge_height = g_move_step_destination_edge_height = 0;
    }
}

/* Step handler of the TELEPORT_ANIMATION phase: once the animation countdown
 * reaches 0, end the path and keep its count in last_path_count. */
static void move_step_update_teleport_animation(unit_t* unit) {
    if (unit->animation.wait == 0) {
        unit->step_phase = MOVE_STEP_PHASE_NONE;
        unit->last_path_count = unit->path.count;
        unit->path.count = 0;
    }
}

/* Step handler of the TELEPORT_START phase. */
static void move_step_update_teleport_start(unit_t* unit) {
    move_step_start_teleport_animation(unit);
}

/* Called for the last step of a path: when the unit has a pending mount, set the
 * step's mount bit (0x10), which marks a step that ends on another unit. */
static void move_step_set_mount_flag(unit_t* unit) {
    if (unit->path.pending_mount == 0) {
        return;
    }
    unit->path.data.steps[unit->movement_path_offset].mount = 1;
}

/*
 * Advance a moving unit along its path by one frame: the full counterpart of
 * move_knockback_update, dispatching every step phase (walk,
 * climb, jump, fly, float and teleport) rather than the knockback subset.
 *
 * The jump-start phases dismount any rider once the jump animation has run
 * out; the two teleport phases play the teleport. When no step is active the
 * path count selects what follows: MOVE_PATH_TELEPORT starts the
 * teleport, MOVE_PATH_TELEPORT_FAILED also clears the pending mount and
 * restarts the AI action, 0 or an exhausted path mounts the pending mount,
 * and otherwise the next path byte starts a step (the path's
 * MOVE_EFFECTIVE_FLY and _FLOAT flags pick the fly and float step
 * starters, and the step's slow bit picks the climb speed).
 */
void move_step_update(unit_t* unit) {
    u32 offset;
    u8 count;
    u8 flags;

    move_step_get_current_and_destination_tiles(unit, &g_move_step_current_tile, &g_move_step_destination_tile);
    g_move_step_value = unit->current_path_step;
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_JUMP_START_SOUTH:
    case MOVE_STEP_PHASE_JUMP_START_EAST:
    case MOVE_STEP_PHASE_JUMP_START_NORTH:
    case MOVE_STEP_PHASE_JUMP_START_WEST:
        if (unit->animation.wait == 0) {
            unit->step_phase++;
            animation_store_facing(ANIMATION_FALL, unit->facing, unit);
            unit_play_sfx(unit, SOUND_SFX_JUMP);
            unit_mount_dismount_rider(unit);
        }
        break;
    case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
    case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
        move_step_update_jump_landing(unit);
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
        move_step_update_jump_ascent(unit);
        /* fallthrough */
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
        move_step_update_jump_flight(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        move_step_update_float_jump(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        move_step_update_walk_to_center(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        move_step_update_float_to_center(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        move_step_update_fly_to_center(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        move_step_update_walk_to_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        move_step_update_float_to_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        move_step_update_fly_to_edge(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_START:
        move_step_update_teleport_start(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_ANIMATION:
        move_step_update_teleport_animation(unit);
        break;
    }

    /* With no step in progress, the path count decides what follows. */
    if (unit->step_phase == MOVE_STEP_PHASE_NONE) {
        count = unit->path.count;
        if (count == MOVE_PATH_TELEPORT) {
            unit_mount_dismount_rider_and_update_display(unit);
            move_step_start_teleport(unit);
        } else if (count == MOVE_PATH_TELEPORT_FAILED) {
            unit_mount_dismount_rider_and_update_display(unit);
            move_step_start_teleport(unit);
            unit->path.pending_mount = 0;
            ai_init_selected_action();
        } else if (count == 0) {
            unit_mount_onto_pending(unit);
        } else if (unit->movement_path_offset < count) {
            if (unit->movement_path_offset == count - 1) {
                move_step_set_mount_flag(unit);
            }

            /* Take the next step and start it with the starter for the movement type. */
            offset = unit->movement_path_offset;
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_move_step_value = unit->path.data.steps[offset];
            move_step_set_slope_scales(unit);
            flags = unit->path.flags;
            if (flags & MOVE_EFFECTIVE_FLY) {
                move_step_start_fly(unit, g_move_step_current_tile, g_move_step_destination_tile);
            } else if (flags & MOVE_EFFECTIVE_FLOAT) {
                move_step_start_float(unit, g_move_step_current_tile, g_move_step_destination_tile);
            } else {
                switch (g_move_step_value.slow) {
                case 0:
                    unit->step_speed = 0x2000;
                    move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
                    break;
                case 1:
                    move_step_start_climb(unit, g_move_step_current_tile, g_move_step_destination_tile);
                    break;
                }
            }
        } else {
            unit_mount_onto_pending(unit);
        }
    }
}

/*
 * Advance a knocked-back or path-moving unit by one frame.
 *
 * While a step is in progress (step_phase non-zero) the walk speed
 * decays by a quarter per frame down to ONE and the step phase handler runs.
 * Once no step is active, the next path byte starts a new step, or
 * move_settle_on_tile ends the move when the path is exhausted.
 */
void move_knockback_update(unit_t* unit) {
    u32 offset;
    s32 speed;

    move_step_get_current_and_destination_tiles(unit, &g_move_step_current_tile, &g_move_step_destination_tile);
    g_move_step_value = unit->current_path_step;
    if (unit->step_phase != MOVE_STEP_PHASE_NONE) {
        speed = unit->walk_speed;
        unit->walk_speed = (speed > ONE) ? speed / 4 * 3 : ONE;
        switch (unit->step_phase) {
        case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
            move_step_update_walk_to_center(unit);
            break;
        case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
        case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
            move_step_update_walk_to_edge(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
        case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
        case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
            move_step_update_jump_landing(unit);
            break;
        case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
            move_step_update_jump_ascent(unit);
            /* fallthrough */
        case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
        case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
        case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
            move_step_update_jump_flight(unit);
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
            move_step_set_slope_scales(unit);
            unit->step_speed = 0x2000;
            move_step_start_walk(unit, g_move_step_current_tile, g_move_step_destination_tile);
        } else {
            move_settle_on_tile(unit);
        }
    }
}

/* Give a knocked-back unit a one-step path to its destination: a plain step in the travel
 * direction with the destination level as its elevation bit, at walk speed 0x8000 (four times the
 * usual 0x2000), which move_knockback_update then slows.
 *
 * The masks edit path.data.steps[0] as bits 8-15 of the path's first word: 0x300 length,
 * 0x400 slow, 0x800 climb, 0x1000 mount, 0x2000 higher and 0xc000 the move_step_direction_e. */
static void move_knockback_init(unit_t* unit) {
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
    knockback_state |= (unit->movement.destination_layer & 1) << 13;
    *path_word = knockback_state;
    direction = move_step_get_direction(unit);

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
void move_knockback_start(const unit_t* src, unit_t* dst) {
    dst->movement.destination_x = src->strike.target_tile.x;
    dst->movement.destination_y = src->strike.target_tile.y;
    dst->movement.destination_layer = src->strike.target_tile.layer;
    move_knockback_init(dst);
}

/* Map the directional buttons to a map direction (battle facing, diagonals included) for the
 * camera's yaw quadrant and the cursor input mode. source picks held, newly pressed (repeating at
 * the cursor repeat speed) or released buttons; returns -1 for no direction or when a face button,
 * Select or Start was just pressed. */
s32 camera_get_input_direction(s32 source) {
    s32 direction = -1;
    u32 input;
    u32 previous;
    u32 pressed;
    u32 divisor;
    u32 threshold;

    /* Pick the button word that source names for the current cursor input mode. */
    switch (g_cursor_input_mode) {
    case CURSOR_INPUT_OPTION_A:
    case CURSOR_INPUT_OPTION_B:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons;
            break;
        case CAMERA_INPUT_PRESSED:
            input = g_input_pressed_buttons;
            switch (g_game_options.fields.cursor_repeat_speed) {
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
            if (threshold < g_input_stable_frames && g_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_input_released_buttons;
            break;
        }
        break;
    case CURSOR_INPUT_FREE_CURSOR:
        switch (source) {
        case CAMERA_INPUT_HELD:
            input = g_input_previous_buttons | g_input_previous_copy_0;
            break;
        case CAMERA_INPUT_PRESSED:
            previous = g_input_previous_buttons | g_input_previous_copy_0;
            pressed = g_input_pressed_buttons;
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
            } else if (g_input_pressed_copy_1 & PSX_PAD_UP) {
                /* state_update_controller_input stores last frame's presses in both copy_0 and
                 * copy_1, so these lone-direction tests never pass: a lone direction registers only
                 * through the auto-repeat below. */
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_UP) && (previous & PSX_PAD_UP)) {
                    input = PSX_PAD_UP;
                }
            } else if (g_input_pressed_copy_1 & PSX_PAD_DOWN) {
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_DOWN) && (previous & PSX_PAD_DOWN)) {
                    input = PSX_PAD_DOWN;
                }
            } else if (g_input_pressed_copy_1 & PSX_PAD_LEFT) {
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_LEFT) && (previous & PSX_PAD_LEFT)) {
                    input = PSX_PAD_LEFT;
                }
            } else if (g_input_pressed_copy_1 & PSX_PAD_RIGHT) {
                if (!((pressed | g_input_pressed_copy_0) & PSX_PAD_RIGHT) && (previous & PSX_PAD_RIGHT)) {
                    input = PSX_PAD_RIGHT;
                }
            }

            switch (g_game_options.fields.cursor_repeat_speed) {
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
            if (threshold < g_input_stable_frames && g_input_stable_frames % divisor == 0) {
                input = g_input_previous_buttons | g_input_previous_copy_0;
            }
            break;
        case CAMERA_INPUT_RELEASED:
            input = g_input_released_buttons;
            break;
        }
        break;
    }

    /* Rotate the buttons by the camera's yaw quadrant into a battle facing for the mode's layout. */
    switch (g_cursor_input_mode) {
    case CURSOR_INPUT_OPTION_A:
        switch (g_camera_render_state.vy & 0xC00) {
        case UNIT_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_EAST;
            }
            break;
        case UNIT_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH;
            }
            break;
        case UNIT_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_WEST;
            }
            break;
        case UNIT_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH;
            }
            break;
        }
        break;
    case CURSOR_INPUT_OPTION_B:
        switch (g_camera_render_state.vy & 0xC00) {
        case UNIT_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH;
            }
            break;
        case UNIT_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_WEST;
            }
            break;
        case UNIT_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH;
            }
            break;
        case UNIT_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_EAST;
            }
            break;
        }
        break;
    case CURSOR_INPUT_FREE_CURSOR:
        switch (g_camera_render_state.vy & 0xC00) {
        case UNIT_FACING_SOUTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_NORTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_WEST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_EAST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH_EAST;
            }
            break;
        case UNIT_FACING_WEST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_WEST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_SOUTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_NORTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_EAST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_NORTH_WEST;
            }
            break;
        case UNIT_FACING_NORTH:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_SOUTH;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_EAST;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_WEST;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_SOUTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH_WEST;
            }
            break;
        case UNIT_FACING_EAST:
            if ((input & (PSX_PAD_UP | PSX_PAD_RIGHT)) == (PSX_PAD_UP | PSX_PAD_RIGHT)) {
                direction = UNIT_FACING_EAST;
            } else if ((input & (PSX_PAD_UP | PSX_PAD_LEFT)) == (PSX_PAD_UP | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_NORTH;
            } else if ((input & (PSX_PAD_RIGHT | PSX_PAD_DOWN)) == (PSX_PAD_RIGHT | PSX_PAD_DOWN)) {
                direction = UNIT_FACING_SOUTH;
            } else if ((input & (PSX_PAD_DOWN | PSX_PAD_LEFT)) == (PSX_PAD_DOWN | PSX_PAD_LEFT)) {
                direction = UNIT_FACING_WEST;
            } else if (input & PSX_PAD_UP) {
                direction = UNIT_FACING_NORTH_EAST;
            } else if (input & PSX_PAD_DOWN) {
                direction = UNIT_FACING_SOUTH_WEST;
            } else if (input & PSX_PAD_LEFT) {
                direction = UNIT_FACING_NORTH_WEST;
            } else if (input & PSX_PAD_RIGHT) {
                direction = UNIT_FACING_SOUTH_EAST;
            }
            break;
        }
        break;
    }

    if (g_input_pressed_buttons
        & (PSX_PAD_TRIANGLE | PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT | PSX_PAD_START)) {
        direction = -1;
    }
    return direction;
}

/* Start the hovered-unit stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void cursor_show_hovered_unit_panel(void) {
    unit_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    s32 mode;
    s32 unit_id;

    if (unit != 0) {
        mode = UNIT_VIEW_MODE_HOVERED_UNIT;
        unit_id = unit->stats->battle_id;
    } else {
        mode = UNIT_VIEW_MODE_TILE_INFO;
        unit_id = 0xFF;
    }
    unit_view_start(mode, unit_id, 0xFF);
}

/* Start the right-side stats panel for the unit on the cursor tile, or the tile
 * info panel when the tile is empty. */
void cursor_show_right_unit_panel(void) {
    unit_t* unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);

    /* Retail looks up the source unit here and ignores it. */
    unit_get_source();
    if (unit != 0) {
        unit_view_start(UNIT_VIEW_MODE_RIGHT_UNIT, 0xFF, unit->stats->battle_id);
    } else {
        unit_view_start(UNIT_VIEW_MODE_TILE_INFO, 0xFF, 0xFF);
    }
}

/* Start the unit panels for a selected target tile: the attack preview for the
 * source unit against the unit on the cursor tile, or the source unit's own stats
 * when the tile is empty. */
void cursor_show_attack_preview_panel(void) {
    unit_t* target = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
    unit_t* source = unit_get_source();

    if (target != 0) {
        unit_view_start(UNIT_VIEW_MODE_ATTACK_PREVIEW, source->stats->battle_id, target->stats->battle_id);
    } else {
        unit_view_start(UNIT_VIEW_MODE_HOVERED_UNIT, source->stats->battle_id, 0xFF);
    }
}

/* Move the cursor while a target is being chosen: onto selected_unit when one is
 * given, otherwise to the tile stored in the unit's cursor command state.
 *
 * When the cursor tile changes, the unit panels are refreshed for the current
 * game state: the cursor unit's panel in the free-cursor, select-move-tile and
 * target-select states, the preview target's panel in the targeting-range state. */
void cursor_move_to_selection(const unit_t* unit, const unit_t* selected_unit) {
    s32 old_x = g_cursor_x;
    s32 old_layer = g_cursor_layer;
    s32 old_y = g_cursor_y;

    if (selected_unit != 0) {
        cursor_move_to_unit(selected_unit);
    } else {
        g_cursor_x = unit->command_state.cursor.target_panel.vx;
        g_cursor_layer = unit->command_state.cursor.target_panel.vy;
        g_cursor_y = unit->command_state.cursor.target_panel.vz;
    }
    if (((g_cursor_x != old_x) | (g_cursor_layer != old_layer) | (g_cursor_y != old_y)) != 0) {
        switch (g_game_state) {
        case GAME_STATE_FREE_CURSOR:
        case GAME_STATE_SELECT_MOVE_TILE:
        case GAME_STATE_TARGET_SELECT:
            cursor_show_hovered_unit_panel();
            break;
        case GAME_STATE_TARGETING_RANGE:
            cursor_show_right_unit_panel();
            break;
        }
    }
}

/* Moves the free cursor one step from the pad direction and reports whether
 * it moved.
 *
 * Before reading the direction, the cursor may hop to the other map level:
 * on START when the multi-height cursor option is STOP, otherwise once the
 * cursor has rested longer than the option's delay (30, 60 or 90 frames). A
 * step onto a blocked tile whose other level is also blocked is undone;
 * otherwise the cursor takes the unblocked level, or the one nearer the old
 * tile's height.
 *
 * `direction` doubles as the level-hop flag, as the target keeps both in one
 * register. The do-while(0) weights the delay's references so that GCC
 * allocates it before `direction`; without it the two swap s0 and s1. The
 * 32-bit option value is copied first so the loop note does not stop the load from
 * being scheduled into the prologue. */
s32 cursor_move_by_input(void) {
    game_options_t options = g_game_options;
    u32 delay;
    s32 direction;
    battle_map_tile_t* current;
    battle_map_tile_t* stepped;
    battle_map_tile_t* opposite;
    battle_map_tile_t* other_level;
    s8 stepped_rise;
    s8 opposite_rise;

    do {
        switch (options.fields.multi_height_cursor_speed) {
        case GAME_MULTI_HEIGHT_CURSOR_SPEED_FAST:
            delay = 30;
            break;
        case GAME_MULTI_HEIGHT_CURSOR_SPEED_REGULAR:
            delay = 60;
            break;
        case GAME_MULTI_HEIGHT_CURSOR_SPEED_SLOW:
        default:
            delay = 90;
            break;
        }
    } while (0);

    direction = 0;
    current = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer);
    if (g_game_options.fields.multi_height_cursor_speed == GAME_MULTI_HEIGHT_CURSOR_SPEED_STOP) {
        if (g_input_pressed_buttons & PSX_PAD_START) {
            other_level = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
            if (!other_level->flags.blocked) {
                direction = 1;
                g_cursor_layer ^= 1;
            }
        }
    } else if (g_cursor_repeat_counter > delay) {
        g_cursor_repeat_counter = 0;
        other_level = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
        if (!other_level->flags.blocked) {
            direction = 1;
            g_cursor_layer ^= 1;
        }
    }

    if (direction == 0) {
        g_cursor_repeat_counter++;
        g_cursor_previous_tile.x = g_cursor_x;
        g_cursor_previous_tile.layer = g_cursor_layer;
        g_cursor_previous_tile.y = g_cursor_y;

        direction = camera_get_input_direction(CAMERA_INPUT_PRESSED);
        switch (direction) {
        case UNIT_FACING_NORTH:
            g_cursor_y++;
            break;
        case UNIT_FACING_SOUTH:
            g_cursor_y--;
            break;
        case UNIT_FACING_WEST:
            g_cursor_x--;
            break;
        case UNIT_FACING_EAST:
            g_cursor_x++;
            break;
        case UNIT_FACING_NORTH_EAST:
            g_cursor_y++;
            g_cursor_x++;
            break;
        case UNIT_FACING_NORTH_WEST:
            g_cursor_y++;
            g_cursor_x--;
            break;
        case UNIT_FACING_SOUTH_EAST:
            g_cursor_x++;
            g_cursor_y--;
            break;
        case UNIT_FACING_SOUTH_WEST:
            g_cursor_y--;
            g_cursor_x--;
            break;
        }

        /* Keep the cursor on the map: a step past the edge goes back, and a step below 0
         * compares as a large unsigned value and resets to 0. */
        if (g_cursor_x == g_battle_map_terrain_width) {
            g_cursor_x--;
        } else if ((u32)g_cursor_x > g_battle_map_terrain_width) {
            g_cursor_x = 0;
        }
        if (g_cursor_y == g_battle_map_terrain_depth) {
            g_cursor_y = g_battle_map_terrain_depth - 1;
        }
        if ((u32)g_cursor_y > g_battle_map_terrain_depth) {
            g_cursor_y = 0;
        }

        /* The tile stepped onto at the cursor's level and the tile on the other level. */
        stepped = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer);
        opposite = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
        if (stepped->flags.blocked) {
            if (opposite->flags.blocked) {
                switch (direction) {
                case UNIT_FACING_NORTH:
                    g_cursor_y--;
                    break;
                case UNIT_FACING_SOUTH:
                    g_cursor_y++;
                    break;
                case UNIT_FACING_WEST:
                    g_cursor_x++;
                    break;
                case UNIT_FACING_EAST:
                    g_cursor_x--;
                    break;
                case UNIT_FACING_NORTH_EAST:
                    g_cursor_y--;
                    g_cursor_x--;
                    break;
                case UNIT_FACING_NORTH_WEST:
                    g_cursor_y--;
                    g_cursor_x++;
                    break;
                case UNIT_FACING_SOUTH_EAST:
                    g_cursor_x--;
                    g_cursor_y++;
                    break;
                case UNIT_FACING_SOUTH_WEST:
                    g_cursor_y++;
                    g_cursor_x++;
                    break;
                }
            } else {
                g_cursor_layer ^= 1;
            }
        } else if (!opposite->flags.blocked) {
            stepped_rise = stepped->height - current->height;
            opposite_rise = opposite->height - current->height;
            if (stepped_rise < 0) {
                stepped_rise = -stepped_rise;
            }
            if (opposite_rise < 0) {
                opposite_rise = -opposite_rise;
            }
            if (opposite_rise < stepped_rise) {
                g_cursor_layer ^= 1;
            }
        }
    }

    if (((g_cursor_x != g_cursor_previous_tile.x) | (g_cursor_layer != g_cursor_previous_tile.layer)
            | (g_cursor_y != g_cursor_previous_tile.y))
        != 0) {
        sound_play_sfx(SOUND_SFX_CURSOR_MOVE);
        g_cursor_repeat_counter = 0;
        switch (g_game_state) {
        case GAME_STATE_FREE_CURSOR:
        case GAME_STATE_SELECT_MOVE_TILE:
        case GAME_STATE_TARGET_SELECT:
            cursor_show_hovered_unit_panel();
            break;
        case GAME_STATE_TARGETING_RANGE:
            cursor_show_right_unit_panel();
            break;
        }
        return 1;
    }
    return 0;
}

/* Start free map panning: held directions move the camera (camera_pan_update) and the
 * camera stops following the cursor tile. */
void camera_pan_enable(void) {
    g_camera_pan_enabled = 1;
}

/* End free map panning: the camera follows the cursor tile again. */
void camera_pan_disable(void) {
    g_camera_pan_enabled = 0;
}

/* Pans the camera with the directional buttons, once per frame of the battle
 * loop, while g_camera_pan_enabled is set (the states that put the
 * buttons in free-cursor mode: unit highlighting, move-area display and ability
 * preview).
 *
 * Each held direction moves the x or map-depth coordinate by 0x4000 (four world
 * units in 20.12) per frame, diagonals both. The position is then clamped to four
 * tiles (0x70000) outside the map on every side: -0x70000 up to
 * g_battle_map_terrain_width or g_battle_map_terrain_depth tiles of 0x1c000 plus 0x70000. */
void camera_pan_update(void) {
    s32* coord;
    s32 limit;

    if (g_camera_pan_enabled != 0) {
        switch (camera_get_input_direction(CAMERA_INPUT_HELD)) {
        case UNIT_FACING_NORTH:
            coord = &g_camera_real_coords.vz;
            *coord += 4 * ONE;
            break;
        case UNIT_FACING_SOUTH:
            coord = &g_camera_real_coords.vz;
            *coord -= 4 * ONE;
            break;
        case UNIT_FACING_WEST:
            coord = &g_camera_real_coords.vx;
            *coord -= 4 * ONE;
            break;
        case UNIT_FACING_EAST:
            coord = &g_camera_real_coords.vx;
            *coord += 4 * ONE;
            break;
        case UNIT_FACING_NORTH_EAST:
            g_camera_real_coords.vz += 4 * ONE;
            g_camera_real_coords.vx += 4 * ONE;
            break;
        case UNIT_FACING_NORTH_WEST:
            g_camera_real_coords.vz += 4 * ONE;
            g_camera_real_coords.vx -= 4 * ONE;
            break;
        case UNIT_FACING_SOUTH_EAST:
            g_camera_real_coords.vz -= 4 * ONE;
            g_camera_real_coords.vx += 4 * ONE;
            break;
        case UNIT_FACING_SOUTH_WEST:
            g_camera_real_coords.vz -= 4 * ONE;
            g_camera_real_coords.vx -= 4 * ONE;
            break;
        }
        {
            s32* x = &g_camera_real_coords.vx;
            limit = (g_battle_map_terrain_width * (BATTLE_MAP_TILE_SIZE * ONE)) + 4 * BATTLE_MAP_TILE_SIZE * ONE;
            if (limit < *x)
                *x = limit;
            if (*x < -(4 * BATTLE_MAP_TILE_SIZE * ONE))
                *x = -(4 * BATTLE_MAP_TILE_SIZE * ONE);
        }
        limit = (g_battle_map_terrain_depth * (BATTLE_MAP_TILE_SIZE * ONE)) + 4 * BATTLE_MAP_TILE_SIZE * ONE;
        if (limit < g_camera_real_coords.vz)
            g_camera_real_coords.vz = limit;
        if (g_camera_real_coords.vz < -(4 * BATTLE_MAP_TILE_SIZE * ONE))
            g_camera_real_coords.vz = -(4 * BATTLE_MAP_TILE_SIZE * ONE);
    }
}

/* Set the destination of a left turn (falling yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
static void camera_rotation_set_left_destination(s32 target_angle) {
    g_camera_previous_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle + 0x200;
    }
}

/* Start a left turn to target_angle from the slowest speed. */
static void camera_rotation_start_left(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_LEFT;
    g_camera_rotation_increment = 1;
    camera_rotation_set_left_destination(target_angle);
}

/* Set the destination of a right turn (rising yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
static void camera_rotation_set_right_destination(s32 target_angle) {
    g_camera_previous_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle - 0x200;
    }
}

/* Start a right turn to target_angle from the slowest speed. */
static void camera_rotation_start_right(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_RIGHT;
    g_camera_rotation_increment = 1;
    camera_rotation_set_right_destination(target_angle);
}

/* Start an automatic turn of angle yaw units (0x400 is a quarter turn;
 * negative turns left) with its sound, unless a rotation is running. */
static void camera_rotation_request(s32 angle) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (angle < 0) {
            camera_rotation_start_left((g_camera_render_state.vy & 0xFE00) + angle);
            sound_play_sfx_on_free_channel(SOUND_SFX_CAMERA_ROTATE_LEFT);
        } else {
            camera_rotation_start_right((g_camera_render_state.vy & 0xFE00) + angle);
            sound_play_sfx_on_free_channel(SOUND_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}

/* L1 starts a quarter turn left and R1 one right, each with its sound, unless
 * a rotation is running. */
void camera_rotation_handle_input(void) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (g_input_pressed_buttons & PSX_PAD_L1) {
            camera_rotation_start_left((g_camera_render_state.vy & 0xFE00) - 0x400);
            sound_play_sfx_on_free_channel(SOUND_SFX_CAMERA_ROTATE_LEFT);
        }
        if (g_input_pressed_buttons & PSX_PAD_R1) {
            camera_rotation_start_right((g_camera_render_state.vy & 0xFE00) + 0x400);
            sound_play_sfx_on_free_channel(SOUND_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}

/* Step the yaw toward its destination by an increment that grows by twice the
 * vsync interval each frame until it passes 0x2f. On arrival the yaw snaps to
 * the destination; while L1 (turning left) or R1 (turning right) is still held
 * outside an event, the next quarter turn starts at the speed reached, and
 * otherwise the rotation ends: both rotate sounds stop and the stop sound
 * plays. */
void camera_rotation_update(void) {
    s32 angle;

    if (g_camera_rotation_action == CAMERA_ROTATION_LEFT) {
        s16* yaw = &g_camera_render_state.vy;
        *yaw -= g_camera_rotation_increment;
        if (*yaw <= g_camera_yaw_target) {
            angle = (u16)g_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_L1) {
                if (g_game_state != GAME_STATE_EVENT) {
                    camera_rotation_set_left_destination((angle & 0xFE0) - 0x400);
                }
            } else {
                g_camera_rotation_action = CAMERA_ROTATION_NONE;
                sound_stop_sfx(SOUND_SFX_CAMERA_ROTATE_RIGHT);
                sound_stop_sfx(SOUND_SFX_CAMERA_ROTATE_LEFT);
                sound_play_sfx_on_free_channel(SOUND_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else if (g_camera_rotation_action == CAMERA_ROTATION_RIGHT) {
        s16* yaw = &g_camera_render_state.vy;
        *yaw += g_camera_rotation_increment;
        if (*yaw >= g_camera_yaw_target) {
            angle = (u16)g_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_R1) {
                if (g_game_state != GAME_STATE_EVENT) {
                    /* Retail continues the right turn through rotate_left. Harmless: the two
                     * differ only for a destination on an axis, which this one never is. */
                    camera_rotation_set_left_destination((angle & 0xFE0) + 0x400);
                }
            } else {
                g_camera_rotation_action = CAMERA_ROTATION_NONE;
                sound_stop_sfx(SOUND_SFX_CAMERA_ROTATE_RIGHT);
                sound_stop_sfx(SOUND_SFX_CAMERA_ROTATE_LEFT);
                sound_play_sfx_on_free_channel(SOUND_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else {
        return;
    }
    if (g_camera_rotation_increment < 0x30) {
        g_camera_rotation_increment = (g_system_vsync_interval * 2) + g_camera_rotation_increment;
    }
}

/* Start a zoom toward ONE (mode 0) or 0xc00 (mode 1). Only zooming in to ONE
 * from below and out to 0xc00 from above pair an action and target that
 * camera_zoom_update steps; the other two cases leave the zoom action
 * set with no progress, which also blocks the L2 toggle. No retail code calls
 * it; the L2 input goes through camera_zoom_toggle. */
void camera_zoom_start(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_zoom.vx > ONE) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            sound_play_sfx(SOUND_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < ONE) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            sound_play_sfx(SOUND_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    case 1:
        if (g_camera_zoom.vx > 0xC00) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            sound_play_sfx(SOUND_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < 0xC00) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            sound_play_sfx(SOUND_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    }
}

/* Toggle the zoom target between BATTLE_MAP_ZOOM_IN and BATTLE_MAP_ZOOM_OUT, starting the
 * matching zoom with its sound; any other target is left alone. The increment
 * restarts at 4 either way. */
static void camera_zoom_toggle(void) {
    if (g_camera_zoom_target == BATTLE_MAP_ZOOM_IN) {
        g_camera_zoom_action = CAMERA_STEP_DECREASE;
        g_camera_zoom_target = BATTLE_MAP_ZOOM_OUT;
        sound_play_sfx(SOUND_SFX_CAMERA_ZOOM_OUT);
    } else if (g_camera_zoom_target == BATTLE_MAP_ZOOM_OUT) {
        g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
        g_camera_zoom_action = CAMERA_STEP_INCREASE;
        sound_play_sfx(SOUND_SFX_CAMERA_ZOOM_IN);
    }
    g_camera_zoom_increment = 4;
}

/* Toggles the zoom when L2 is newly pressed and no zoom is running. */
void camera_zoom_handle_input(void) {
    if (g_camera_zoom_action == CAMERA_STEP_NONE) {
        if ((g_input_pressed_buttons & PSX_PAD_L2) != 0) {
            camera_zoom_toggle();
        }
    }
}

/* Step the three zoom scales toward the zoom target, ONE zooming in or 0xc00
 * zooming out, by an increment that grows by four times the vsync interval
 * each frame until it reaches 0x300. The zoom ends on the target. */
void camera_zoom_update(void) {
    s32 increment;
    s32* zoom_ptr;

    if (g_camera_zoom_action == CAMERA_STEP_INCREASE && g_camera_zoom_target == BATTLE_MAP_ZOOM_IN) {
        s32 zoom;
        s32 below;
        s32 pan_x;
        s32 pan_y;

        zoom_ptr = &g_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_camera_zoom_increment;
        zoom = zoom + increment;
        *zoom_ptr = zoom;
        below = zoom < ONE;
        pan_x = g_camera_zoom.vy;
        pan_y = g_camera_zoom.vz;
        pan_x = pan_x + increment;
        pan_y = pan_y + increment;
        g_camera_zoom.vy = pan_x;
        g_camera_zoom.vz = pan_y;
        if (!below) {
            g_camera_zoom_action = CAMERA_STEP_NONE;
            g_camera_zoom.vz = ONE;
            g_camera_zoom.vy = ONE;
            *zoom_ptr = ONE;
        }
    } else if (g_camera_zoom_action == CAMERA_STEP_DECREASE
        /* Retail compares the target with the action already loaded: CAMERA_STEP_DECREASE equals
         * BATTLE_MAP_ZOOM_OUT. */
        && g_camera_zoom_target == g_camera_zoom_action) {
        s32 zoom;
        s32 below;

        zoom_ptr = &g_camera_zoom.vx;
        zoom = *zoom_ptr;
        increment = g_camera_zoom_increment;
        zoom = zoom - increment;
        *zoom_ptr = zoom;
        below = zoom < 0xC01;
        g_camera_zoom.vy -= increment;
        g_camera_zoom.vz -= increment;
        if (below) {
            g_camera_zoom_action = CAMERA_STEP_NONE;
            g_camera_zoom.vz = 0xC00;
            g_camera_zoom.vy = 0xC00;
            *zoom_ptr = 0xC00;
        }
    } else {
        return;
    }
    if (increment < 0x300) {
        g_camera_zoom_increment = g_system_vsync_interval * 4 + increment;
    }
}

/* Start a tilt toward the low pitch 0x12e (mode 0) or the high pitch 0x1c0
 * (mode 1) from whichever side the pitch is on. No retail code calls it; the
 * R2 input goes through camera_tilt_toggle. */
void camera_tilt_start(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_render_state.vx >= 0x12F) {
            g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = 1;
            sound_play_sfx(SOUND_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x12E) {
            g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_camera_tilt_increment = 1;
            sound_play_sfx(SOUND_SFX_CAMERA_TILT);
        }
        break;
    case 1:
        if (g_camera_render_state.vx >= 0x1C1) {
            g_camera_tilt_target = BATTLE_MAP_TILT_HIGH;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = 1;
            sound_play_sfx(SOUND_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x1C0) {
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_camera_tilt_target = BATTLE_MAP_TILT_HIGH;
            g_camera_tilt_increment = 1;
            sound_play_sfx(SOUND_SFX_CAMERA_TILT);
        }
        break;
    }
}

/* Toggle the tilt target between low and high, starting the tilt at increment 1
 * with its sound. Dormant targets 3 and 4 advance 3 -> 4 -> LOW; no named writer selects 3. */
static void camera_tilt_toggle(void) {
    s32 target = g_camera_tilt_target;

    if (target == BATTLE_MAP_TILT_LOW) {
        g_camera_tilt_action = CAMERA_STEP_INCREASE;
        g_camera_tilt_target = BATTLE_MAP_TILT_HIGH;
    } else if (target == BATTLE_MAP_TILT_HIGH) {
        g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
        g_camera_tilt_action = CAMERA_STEP_DECREASE;
    } else if (target == 3) {
        g_camera_tilt_action = CAMERA_STEP_DECREASE;
        g_camera_tilt_target = 4;
    } else if (target == 4) {
        g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
        g_camera_tilt_action = CAMERA_STEP_INCREASE;
    }
    g_camera_tilt_increment = 1;
    sound_play_sfx(SOUND_SFX_CAMERA_TILT);
}

/* Toggles the tilt when R2 is newly pressed and no tilt is running. */
void camera_tilt_handle_input(void) {
    if (g_camera_tilt_action == CAMERA_STEP_NONE) {
        if ((g_input_pressed_buttons & PSX_PAD_R2) != 0) {
            camera_tilt_toggle();
        }
    }
}

/* Step the pitch toward the tilt target, 0x1c0 (high) or 0x12e (low), from
 * either side, by an increment that grows by twice the vsync interval each
 * frame until it passes 0x2f. The tilt ends on the target. */
void camera_tilt_update(void) {
    s16 tilt;

    if (g_camera_tilt_action == CAMERA_STEP_INCREASE
        /* Retail compares the target with the action already loaded: CAMERA_STEP_INCREASE equals
         * BATTLE_MAP_TILT_HIGH. */
        && g_camera_tilt_target == g_camera_tilt_action) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state + g_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x1C0) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x1C0;
        }
    } else if (g_camera_tilt_action == CAMERA_STEP_DECREASE && g_camera_tilt_target == BATTLE_MAP_TILT_LOW) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state - g_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x12F) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x12E;
        }
    } else if (g_camera_tilt_action == CAMERA_STEP_DECREASE && g_camera_tilt_target == BATTLE_MAP_TILT_HIGH) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state - g_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x1C1) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x1C0;
        }
    } else if (g_camera_tilt_action == CAMERA_STEP_INCREASE && g_camera_tilt_target == BATTLE_MAP_TILT_LOW) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state + g_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x12E) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x12E;
        }
    } else {
        return;
    }
    if (g_camera_tilt_increment < 0x30) {
        g_camera_tilt_increment += g_system_vsync_interval * 2;
    }
}

/* Aims the camera step vector from the camera position at the acting unit: the
 * unit-length direction (ONE) with its height component multiplied by four. */
static void camera_calculate_acting_unit_vector_normal(void) {
    VECTOR delta;
    unit_t* unit = unit_get_source();

    setVector(&delta, unit->world_coords.vx - g_camera_real_coords.vx / ONE,
        unit->world_coords.vy - g_camera_real_coords.vy / ONE, unit->world_coords.vz - g_camera_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vy = g_camera_step_vector.vy << 2;
}

/* The acting-unit camera step vector at double length, used when the unit is
 * outside the wider screen window. */
static void camera_shift_acting_unit_vector(void) {
    camera_calculate_acting_unit_vector_normal();
    g_camera_step_vector.vx *= 2;
    g_camera_step_vector.vy *= 2;
    g_camera_step_vector.vz *= 2;
}

/* Aim the camera vector from the current camera focus at the cursor tile.
 *
 * The tile surface height is (height + depth) * 12 + half-height * 6 world
 * units above the floor, negated because world Y points down. The focus
 * offset is normalized by VectorNormal into g_camera_step_vector and
 * tripled. Returns the negated tile height. */
static s32 camera_calculate_cursor_tile_vector_normal(void) {
    battle_map_tile_t* tile = battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer);
    SVECTOR tile_position;
    VECTOR delta;
    s32 height;
    battle_map_tile_depth_t depth;

    /* Retail also looks up the tile on the other level and ignores it. */
    battle_map_tile_get_pointer(g_cursor_x, g_cursor_y, g_cursor_layer ^ 1);
    depth = tile->depth_half_height;
    height
        = -((tile->height + depth.depth) * BATTLE_MAP_HEIGHT_STEP + depth.half_height * (BATTLE_MAP_HEIGHT_STEP / 2));
    vector_set_svector(&tile_position, g_cursor_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), height,
        g_cursor_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
    setVector(&delta, tile_position.vx - g_camera_real_coords.vx / ONE, height - g_camera_real_coords.vy / ONE,
        tile_position.vz - g_camera_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vx *= 3;
    g_camera_step_vector.vy *= 3;
    g_camera_step_vector.vz *= 3;
    return height;
}

/* Doubles the cursor tile vector computed by
 * camera_calculate_cursor_tile_vector_normal and passes that routine's
 * result through; callers hand it to camera_step_focus_toward_cursor_tile. */
static s32 camera_scale_cursor_tile_vector(void) {
    s32 result = camera_calculate_cursor_tile_vector_normal();

    g_camera_step_vector.vx *= 2;
    g_camera_step_vector.vy *= 2;
    g_camera_step_vector.vz *= 2;
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
static void camera_step_focus_toward_cursor_tile(s16 height) {
    CAMERA_STEP_TOWARD(&g_camera_real_coords.vx, g_camera_step_vector.vx,
        g_cursor_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
    CAMERA_STEP_TOWARD(&g_camera_real_coords.vy, g_camera_step_vector.vy, height);
    CAMERA_STEP_TOWARD(&g_camera_real_coords.vz, g_camera_step_vector.vz,
        g_cursor_y * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2));
}

/* Moves the camera position by this frame's step vector (g_camera_step_vector)
 * toward the unit's position without passing it.
 *
 * Each component of g_camera_real_coords (20.12) advances only
 * while it is still short of the target in the step's direction, and is set to
 * the target exactly when the step carries it past. Called for the acting unit
 * while it moves, chooses its wait direction or starts an attack animation. */
static void camera_real_coords_step_toward_target(const unit_t* target) {
    if (g_camera_step_vector.vx < 0) {
        if (g_camera_real_coords.vx / ONE > target->world_coords.vx) {
            g_camera_real_coords.vx += g_camera_step_vector.vx;
            if (g_camera_real_coords.vx / ONE < target->world_coords.vx) {
                g_camera_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    } else {
        if (g_camera_real_coords.vx / ONE < target->world_coords.vx) {
            g_camera_real_coords.vx += g_camera_step_vector.vx;
            if (g_camera_real_coords.vx / ONE > target->world_coords.vx) {
                g_camera_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    }
    if (g_camera_step_vector.vy < 0) {
        if (g_camera_real_coords.vy / ONE > target->world_coords.vy) {
            g_camera_real_coords.vy += g_camera_step_vector.vy;
            if (g_camera_real_coords.vy / ONE < target->world_coords.vy) {
                g_camera_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    } else {
        if (g_camera_real_coords.vy / ONE < target->world_coords.vy) {
            g_camera_real_coords.vy += g_camera_step_vector.vy;
            if (g_camera_real_coords.vy / ONE > target->world_coords.vy) {
                g_camera_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    }
    if (g_camera_step_vector.vz < 0) {
        if (g_camera_real_coords.vz / ONE > target->world_coords.vz) {
            g_camera_real_coords.vz += g_camera_step_vector.vz;
            if (g_camera_real_coords.vz / ONE < target->world_coords.vz) {
                g_camera_real_coords.vz = target->world_coords.vz * ONE;
            }
        }
    } else {
        if (g_camera_real_coords.vz / ONE < target->world_coords.vz) {
            /* Retail quirk: only this positive map-depth step doubles the component. */
            g_camera_real_coords.vz += g_camera_step_vector.vz * 2;
            if (g_camera_real_coords.vz / ONE > target->world_coords.vz) {
                g_camera_real_coords.vz = target->world_coords.vz * ONE;
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
 * GAME_STATE_ACTION_ANNOUNCEMENT take the idle path of
 * camera_update_cursor_tile_vector, and that state and those above it
 * clear the vector.
 *
 * The idle path is camera_update_cursor_tile_vector verbatim. The four
 * vector_set_vector calls are one tail-merged block in the target, placed
 * last so it falls into the epilogue, so the idle path's window test is
 * spelled inverted with the clear-vector call as the function's last
 * statement. Written the natural way, the merged block lands mid-function
 * with an extra `j`. */
void camera_update_vector_from_game_state(void) {
    unit_t* unit;
    s32 height;

    if (g_game_state == GAME_STATE_POST_MOVE_EVENTS) {
        unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        height = 0;
        if (unit != 0) {
            height = (g_sprite_sheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_cursor_screen_x >= 0xF9 && g_cursor_screen_x < 0x108 && g_cursor_screen_y - height >= 0xA1
            && g_cursor_screen_y < 0xB0) {
            vector_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        /* The scaling routine passes through the normal routine's height bound. */
        camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
        return;
    }
    if (g_game_state == GAME_STATE_UNIT_MOVING || g_game_state == GAME_STATE_WAIT_DIRECTION
        || g_game_state == GAME_STATE_PRE_ATTACK_ANIMATION) {
        unit = unit_get_source();
        height = (g_sprite_sheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        if (unit->camera_relative_x >= 0xB1 && unit->camera_relative_x < 0x150
            && unit->camera_relative_y - height >= 0x51 && unit->camera_relative_y < 0xA0) {
            vector_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (unit->camera_relative_x >= 0xA1 && unit->camera_relative_x < 0x160
            && unit->camera_relative_y - height >= 0x41 && unit->camera_relative_y < 0xB0) {
            camera_calculate_acting_unit_vector_normal();
        } else {
            camera_shift_acting_unit_vector();
        }
        camera_real_coords_step_toward_target(unit);
        return;
    }
    if (g_game_state >= GAME_STATE_ACTION_ANNOUNCEMENT) {
        vector_set_vector(&g_camera_step_vector, 0, 0, 0);
        return;
    }
    if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_pan_enabled == 0) {
        unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        height = 0;
        if (unit != 0) {
            height = (g_sprite_sheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_cursor_screen_x < 0xB1 || g_cursor_screen_x >= 0x150 || g_cursor_screen_y - height < 0x51
            || g_cursor_screen_y >= 0xA0) {
            if (g_cursor_screen_x >= 0xA1 && g_cursor_screen_x < 0x160 && g_cursor_screen_y - height >= 0x41
                && g_cursor_screen_y < 0xB0) {
                camera_step_focus_toward_cursor_tile(camera_calculate_cursor_tile_vector_normal());
            } else {
                /* The scaling routine passes the normal routine's result through. */
                camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
            }
            return;
        }
        vector_set_vector(&g_camera_step_vector, 0, 0, 0);
    }
}

/* Refresh the cursor-tile camera vector while the map camera is idle (no zoom,
 * tilt, rotation or free pan).
 *
 * Half the cursor unit's sprite height (scaled by g_camera_zoom.vy, 4.12
 * fixed point) raises the cursor's screen position (g_cursor_screen_x,
 * g_cursor_screen_y) to the unit's middle. Inside the inner window
 * (x 0xb1..0x14f, y 0x51..0x9f) the camera holds still; inside the outer
 * window, 0x10 pixels wider on every side, it drifts toward the cursor tile
 * at the normal speed, and beyond that at double speed. The tile height either
 * vector routine returns is the height bound for
 * camera_step_focus_toward_cursor_tile. */
void camera_update_cursor_tile_vector(void) {
    unit_t* unit;
    s32 height;

    if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_pan_enabled == 0) {
        unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        height = 0;
        if (unit != 0) {
            height = (g_sprite_sheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_cursor_screen_x >= 0xB1 && g_cursor_screen_x < 0x150 && g_cursor_screen_y - height >= 0x51
            && g_cursor_screen_y < 0xA0) {
            vector_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (g_cursor_screen_x >= 0xA1 && g_cursor_screen_x < 0x160 && g_cursor_screen_y - height >= 0x41
            && g_cursor_screen_y < 0xB0) {
            camera_step_focus_toward_cursor_tile(camera_calculate_cursor_tile_vector_normal());
        } else {
            /* The scaling routine passes through the normal routine's height bound. */
            camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
        }
    }
}

/* Advance the movement of each runtime unit that has a path. Runs every frame of event mode and of
 * the change-map jump-in state. */
void move_step_update_all_units(void) {
    s32 i;
    unit_t* unit;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = unit_get_by_runtime_id(i);
        if (unit != 0 && unit->path.count != 0) {
            move_step_update(unit);
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
void camera_rotate_when_unit_tile_not_visible(const unit_t* unit) {
    s32 direction;
    u8 flags;
    battle_map_tile_t* tile = battle_map_tile_get_pointer(unit->map_x, unit->map_y, unit->map_layer);

    direction = (0 - g_camera_render_state.vy) & 0xFFF;
    if (direction < 0) {
        direction += 0x3FF;
    }
    direction >>= 0xA;
    if (g_camera_render_state.vx >= CAMERA_PITCH_STEEP_MASK_THRESHOLD) {
        flags = tile->camera_block_masks >> BATTLE_MAP_TILE_CAMERA_BLOCK_STEEP_SHIFT;
    } else {
        flags = tile->camera_block_masks & BATTLE_MAP_TILE_CAMERA_BLOCK_DIRECTION_MASK;
    }
    switch (direction) {
    case 0:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                camera_rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    camera_rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    camera_rotation_request(0x800);
                }
            }
        }
        break;
    case 1:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                camera_rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    camera_rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    camera_rotation_request(0x800);
                }
            }
        }
        break;
    case 2:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                camera_rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    camera_rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    camera_rotation_request(0x800);
                }
            }
        }
        break;
    case 3:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                camera_rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    camera_rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    camera_rotation_request(0x800);
                }
            }
        }
        break;
    }
}
