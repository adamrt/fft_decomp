/* Pan, rotation, zoom, tilt and the focus vectors. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/main.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Let the held pad directions pan the camera across the map
 * (camera_update_map_pan); the camera stops following the cursor tile. */
void camera_enable_map_pan(void) {
    g_camera_map_pan_enabled = 1;
}

/* End free map panning: the camera follows the cursor tile again. */
void camera_disable_map_pan(void) {
    g_camera_map_pan_enabled = 0;
}

/* Pans the camera with the directional buttons, once per frame of the battle
 * loop, while g_camera_map_pan_enabled is set (the states that put the
 * buttons in free-cursor mode: unit highlighting, move-area display and ability
 * preview).
 *
 * Each held direction moves the x or map-depth coordinate by 0x4000 (four world
 * units in 20.12) per frame, diagonals both. The position is then clamped to four
 * tiles (0x70000) outside the map on every side: -0x70000 up to
 * g_map_max_x or g_map_max_y tiles of 0x1c000 plus 0x70000. */
void camera_update_map_pan(void) {
    s32* coord;
    s32 limit;

    if (g_camera_map_pan_enabled != 0) {
        switch (camera_get_input_direction(CAMERA_INPUT_HELD)) {
        case BATTLE_FACING_NORTH:
            coord = &g_camera_current_real_coords.vz;
            *coord += 4 * ONE;
            break;
        case BATTLE_FACING_SOUTH:
            coord = &g_camera_current_real_coords.vz;
            *coord -= 4 * ONE;
            break;
        case BATTLE_FACING_WEST:
            coord = &g_camera_current_real_coords.vx;
            *coord -= 4 * ONE;
            break;
        case BATTLE_FACING_EAST:
            coord = &g_camera_current_real_coords.vx;
            *coord += 4 * ONE;
            break;
        case BATTLE_FACING_NORTH_EAST: {
            s32* y = &g_camera_current_real_coords.vz;
            *y += 4 * ONE;
            g_camera_current_real_coords.vx += 4 * ONE;
            break;
        }
        case BATTLE_FACING_NORTH_WEST: {
            s32* y = &g_camera_current_real_coords.vz;
            *y += 4 * ONE;
            g_camera_current_real_coords.vx -= 4 * ONE;
            break;
        }
        case BATTLE_FACING_SOUTH_EAST: {
            s32* y = &g_camera_current_real_coords.vz;
            *y -= 4 * ONE;
            g_camera_current_real_coords.vx += 4 * ONE;
            break;
        }
        case BATTLE_FACING_SOUTH_WEST: {
            s32* y = &g_camera_current_real_coords.vz;
            *y -= 4 * ONE;
            g_camera_current_real_coords.vx -= 4 * ONE;
            break;
        }
        }
        {
            s32* x = &g_camera_current_real_coords.vx;
            limit = (g_map_max_x * (MAP_TILE_SIZE * ONE)) + 4 * MAP_TILE_SIZE * ONE;
            if (limit < *x)
                *x = limit;
            if (*x < -(4 * MAP_TILE_SIZE * ONE))
                *x = -(4 * MAP_TILE_SIZE * ONE);
        }
        limit = (g_map_max_y * (MAP_TILE_SIZE * ONE)) + 4 * MAP_TILE_SIZE * ONE;
        if (limit < g_camera_current_real_coords.vz)
            g_camera_current_real_coords.vz = limit;
        if (g_camera_current_real_coords.vz < -(4 * MAP_TILE_SIZE * ONE))
            g_camera_current_real_coords.vz = -(4 * MAP_TILE_SIZE * ONE);
    }
}

/* Set the destination of a left turn (falling yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
void camera_rotate_left(s32 target_angle) {
    g_battle_previous_camera_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle + 0x200;
    }
}

/* Start a left turn to target_angle from the slowest speed. */
void camera_start_rotation_left(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_LEFT;
    g_camera_rotation_increment = 1;
    camera_rotate_left(target_angle);
}

/* Set the destination of a right turn (rising yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
void camera_rotate_right(s32 target_angle) {
    g_battle_previous_camera_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle - 0x200;
    }
}

/* Start a right turn to target_angle from the slowest speed. */
void camera_start_rotation_right(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_RIGHT;
    g_camera_rotation_increment = 1;
    camera_rotate_right(target_angle);
}

/* Start an automatic turn of angle yaw units (0x400 is a quarter turn;
 * negative turns left) with its sound, unless a rotation is running. */
void camera_request_rotation(s32 angle) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (angle < 0) {
            camera_start_rotation_left((g_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_LEFT);
        } else {
            camera_start_rotation_right((g_camera_render_state.vy & 0xFE00) + angle);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}

/* L1 starts a quarter turn left and R1 one right, each with its sound, unless
 * a rotation is running. */
void camera_handle_rotation_input(void) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (g_main_input_pressed_buttons & PSX_PAD_L1) {
            camera_start_rotation_left((g_camera_render_state.vy & 0xFE00) - 0x400);
            main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_LEFT);
        }
        if (g_main_input_pressed_buttons & PSX_PAD_R1) {
            camera_start_rotation_right((g_camera_render_state.vy & 0xFE00) + 0x400);
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
void camera_update_rotation(void) {
    s32 angle;

    if (g_camera_rotation_action == CAMERA_ROTATION_LEFT) {
        s16* yaw = &g_camera_render_state.vy;
        *yaw -= g_camera_rotation_increment;
        if (*yaw <= g_camera_yaw_target) {
            angle = (u16)g_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_L1) {
                if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
                    camera_rotate_left((angle & 0xFE0) - 0x400);
                }
            } else {
                g_camera_rotation_action = CAMERA_ROTATION_NONE;
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_RIGHT);
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_LEFT);
                main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else if (g_camera_rotation_action == CAMERA_ROTATION_RIGHT) {
        s16* yaw = &g_camera_render_state.vy;
        *yaw += g_camera_rotation_increment;
        if (*yaw >= g_camera_yaw_target) {
            angle = (u16)g_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_R1) {
                if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
                    camera_rotate_left((angle & 0xFE0) + 0x400);
                }
            } else {
                g_camera_rotation_action = CAMERA_ROTATION_NONE;
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_RIGHT);
                main_sound_stop_sfx(MAIN_SFX_CAMERA_ROTATE_LEFT);
                main_sound_play_sfx_find_channel(MAIN_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else {
        return;
    }
    if (g_camera_rotation_increment < 0x30) {
        g_camera_rotation_increment = (g_main_system_animation_speed * 2) + g_camera_rotation_increment;
    }
}

/* Start a zoom toward ONE (mode 0) or 0xc00 (mode 1) from whichever side the
 * scale is on. No retail code calls it; the L2 input goes through
 * camera_toggle_zoom. */
void camera_zoom_map(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_zoom.vx > ONE) {
            g_map_zoom_target = MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < ONE) {
            g_map_zoom_target = MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    case 1:
        if (g_camera_zoom.vx > 0xC00) {
            g_map_zoom_target = MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < 0xC00) {
            g_map_zoom_target = MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    }
}

/* Toggle the battle map zoom between "in" (target=1, action=2) and
   "out" (target=4, action=4), playing the corresponding zoom SFX. Any
   other current zoom target is left alone. */
void camera_toggle_zoom(void) {
    if (g_map_zoom_target == MAP_ZOOM_IN) {
        g_camera_zoom_action = CAMERA_STEP_DECREASE;
        g_map_zoom_target = MAP_ZOOM_OUT;
        main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_OUT);
    } else if (g_map_zoom_target == MAP_ZOOM_OUT) {
        g_map_zoom_target = MAP_ZOOM_IN;
        g_camera_zoom_action = CAMERA_STEP_INCREASE;
        main_sound_play_sfx(MAIN_SFX_CAMERA_ZOOM_IN);
    }
    g_camera_zoom_increment = 4;
}

/* Toggles the zoom when L2 is newly pressed and no zoom is running. */
void camera_update_zoom_input(void) {
    if (g_camera_zoom_action == CAMERA_STEP_NONE) {
        if ((g_main_input_pressed_buttons & PSX_PAD_L2) != 0) {
            camera_toggle_zoom();
        }
    }
}

/* Step the three zoom scales toward the zoom target, ONE zooming in or 0xc00
 * zooming out, by an increment that grows by four times the animation speed
 * each frame until it reaches 0x300. The zoom ends on the target. */
void camera_update_zoom(void) {
    s32 increment;
    s32* zoom_ptr;

    if (g_camera_zoom_action == CAMERA_STEP_INCREASE && g_map_zoom_target == MAP_ZOOM_IN) {
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
    } else if (g_camera_zoom_action == CAMERA_STEP_DECREASE && g_map_zoom_target == g_camera_zoom_action) {
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
        g_camera_zoom_increment = g_main_system_animation_speed * 4 + increment;
    }
}

/* Start a tilt toward the low pitch 0x12e (mode 0) or the high pitch 0x1c0
 * (mode 1) from whichever side the pitch is on. No retail code calls it; the
 * R2 input goes through camera_toggle_tilt. */
void camera_handle_tilt_request(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_render_state.vx >= 0x12F) {
            g_map_tilt_target = MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x12E) {
            g_map_tilt_target = MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_camera_tilt_increment = 1;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    case 1:
        if (g_camera_render_state.vx >= 0x1C1) {
            g_map_tilt_target = MAP_TILT_HIGH;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x1C0) {
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_map_tilt_target = MAP_TILT_HIGH;
            g_camera_tilt_increment = mode;
            main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
        }
        break;
    }
}

/* Toggle the camera tilt and play its sound. */
void camera_toggle_tilt(void) {
    s32 target;

    target = g_map_tilt_target;
    if (target == MAP_TILT_LOW) {
        g_camera_tilt_action = CAMERA_STEP_INCREASE;
        g_map_tilt_target = MAP_TILT_HIGH;
    } else if (target == MAP_TILT_HIGH) {
        g_map_tilt_target = MAP_TILT_LOW;
        g_camera_tilt_action = CAMERA_STEP_DECREASE;
    } else if (target == 3) {
        g_camera_tilt_action = CAMERA_STEP_DECREASE;
        g_map_tilt_target = 4;
    } else if (target == 4) {
        g_map_tilt_target = MAP_TILT_LOW;
        g_camera_tilt_action = CAMERA_STEP_INCREASE;
    }
    g_camera_tilt_increment = 1;
    main_sound_play_sfx(MAIN_SFX_CAMERA_TILT);
}

/* Toggles the tilt when R2 is newly pressed and no tilt is running. */
void camera_update_tilt_input(void) {
    if (g_camera_tilt_action == CAMERA_STEP_NONE) {
        if ((g_main_input_pressed_buttons & PSX_PAD_R2) != 0) {
            camera_toggle_tilt();
        }
    }
}

/* Step the pitch toward the tilt target, 0x1c0 (high) or 0x12e (low), from
 * either side, by an increment that grows by twice the animation speed each
 * frame until it passes 0x2f. The tilt ends on the target. */
void camera_update_tilt(void) {
    s16 tilt;

    if (g_camera_tilt_action == CAMERA_STEP_INCREASE && g_map_tilt_target == g_camera_tilt_action) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state + g_camera_tilt_increment;
        *state = tilt;
        if (tilt >= 0x1C0) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x1C0;
        }
    } else if (g_camera_tilt_action == CAMERA_STEP_DECREASE && g_map_tilt_target == MAP_TILT_LOW) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state - g_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x12F) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x12E;
        }
    } else if (g_camera_tilt_action == CAMERA_STEP_DECREASE && g_map_tilt_target == MAP_TILT_HIGH) {
        s16* state = &g_camera_render_state.vx;
        tilt = *state - g_camera_tilt_increment;
        *state = tilt;
        if (tilt < 0x1C1) {
            g_camera_tilt_action = CAMERA_STEP_NONE;
            *state = 0x1C0;
        }
    } else if (g_camera_tilt_action == CAMERA_STEP_INCREASE && g_map_tilt_target == MAP_TILT_LOW) {
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
        g_camera_tilt_increment += g_main_system_animation_speed * 2;
    }
}

/* Aims the camera step vector from the camera position at the acting unit: the
 * unit-length direction (ONE) with its height component multiplied by four. */
void camera_calculate_acting_unit_vector_normal(void) {
    VECTOR delta;
    battle_unit_t* unit;

    unit = battle_unit_get_source();
    setVector(&delta, unit->world_coords.vx - g_camera_current_real_coords.vx / ONE,
        unit->world_coords.vy - g_camera_current_real_coords.vy / ONE,
        unit->world_coords.vz - g_camera_current_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vy = g_camera_step_vector.vy << 2;
}

/* The acting-unit camera step vector at double length, used when the unit is
 * outside the wider screen window. */
void camera_shift_acting_unit_vector(void) {
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
s32 camera_calculate_cursor_tile_vector_normal(void) {
    map_tile_t* tile;
    SVECTOR tile_position;
    VECTOR delta;
    s32 height;
    map_tile_depth_t depth;

    tile = map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    map_get_tile_data_pointer(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z ^ 1);
    depth = tile->depth_half_height;
    height = -((tile->height + depth.depth) * MAP_HEIGHT_STEP + depth.half_height * (MAP_HEIGHT_STEP / 2));
    main_util_set_svector(&tile_position, g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), height,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    setVector(&delta, tile_position.vx - g_camera_current_real_coords.vx / ONE,
        height - g_camera_current_real_coords.vy / ONE, tile_position.vz - g_camera_current_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vx *= 3;
    g_camera_step_vector.vy *= 3;
    g_camera_step_vector.vz *= 3;
    return height;
}

/* Doubles the cursor tile vector computed by
 * camera_calculate_cursor_tile_vector_normal and passes that routine's
 * result through; callers hand it to camera_step_focus_toward_cursor_tile. */
s32 camera_scale_cursor_tile_vector(void) {
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
void camera_step_focus_toward_cursor_tile(s16 height) {
    CAMERA_STEP_TOWARD(&g_camera_current_real_coords.vx, g_camera_step_vector.vx,
        g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    CAMERA_STEP_TOWARD(&g_camera_current_real_coords.vy, g_camera_step_vector.vy, height);
    CAMERA_STEP_TOWARD(&g_camera_current_real_coords.vz, g_camera_step_vector.vz,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
}

/* Moves the camera position by this frame's step vector (g_camera_step_vector)
 * toward the unit's position without passing it.
 *
 * Each component of g_camera_current_real_coords (20.12) advances only
 * while it is still short of the target in the step's direction, and is set to
 * the target exactly when the step carries it past. Called for the acting unit
 * while it moves, chooses its wait direction or starts an attack animation. */
void camera_step_real_coords_toward_target(const battle_unit_t* target) {
    if (g_camera_step_vector.vx < 0) {
        if (g_camera_current_real_coords.vx / ONE > target->world_coords.vx) {
            g_camera_current_real_coords.vx += g_camera_step_vector.vx;
            if (g_camera_current_real_coords.vx / ONE < target->world_coords.vx) {
                g_camera_current_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    } else {
        if (g_camera_current_real_coords.vx / ONE < target->world_coords.vx) {
            g_camera_current_real_coords.vx += g_camera_step_vector.vx;
            if (g_camera_current_real_coords.vx / ONE > target->world_coords.vx) {
                g_camera_current_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    }
    if (g_camera_step_vector.vy < 0) {
        if (g_camera_current_real_coords.vy / ONE > target->world_coords.vy) {
            g_camera_current_real_coords.vy += g_camera_step_vector.vy;
            if (g_camera_current_real_coords.vy / ONE < target->world_coords.vy) {
                g_camera_current_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    } else {
        if (g_camera_current_real_coords.vy / ONE < target->world_coords.vy) {
            g_camera_current_real_coords.vy += g_camera_step_vector.vy;
            if (g_camera_current_real_coords.vy / ONE > target->world_coords.vy) {
                g_camera_current_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    }
    if (g_camera_step_vector.vz < 0) {
        if (g_camera_current_real_coords.vz / ONE > target->world_coords.vz) {
            g_camera_current_real_coords.vz += g_camera_step_vector.vz;
            if (g_camera_current_real_coords.vz / ONE < target->world_coords.vz) {
                g_camera_current_real_coords.vz = target->world_coords.vz * ONE;
            }
        }
    } else {
        if (g_camera_current_real_coords.vz / ONE < target->world_coords.vz) {
            /* Retail quirk: only this positive map-depth step doubles the component. */
            g_camera_current_real_coords.vz += g_camera_step_vector.vz * 2;
            if (g_camera_current_real_coords.vz / ONE > target->world_coords.vz) {
                g_camera_current_real_coords.vz = target->world_coords.vz * ONE;
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
 * camera_update_cursor_tile_vector, and that state and those above it
 * clear the vector.
 *
 * The scaling routine passes the normal routine's result through, and these
 * callers hand it on.
 *
 * The idle path is camera_update_cursor_tile_vector verbatim. The four
 * main_util_set_vector calls are one tail-merged block in the target, placed
 * last so it falls into the epilogue, so the idle path's window test is
 * spelled inverted with the clear-vector call as the function's last
 * statement. Written the natural way, the merged block lands mid-function
 * with an extra `j`. */
void camera_update_vector_from_game_state(void) {
    battle_unit_t* unit;
    s32 height;

    if (g_battle_game_state == BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xF9 && g_battle_cursor_screen_x < 0x108
            && g_battle_cursor_screen_y - height >= 0xA1 && g_battle_cursor_screen_y < 0xB0) {
            main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        /* The scaling routine passes through the normal routine's height bound. */
        camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
        return;
    }
    if (g_battle_game_state == BATTLE_GAME_STATE_UNIT_MOVING || g_battle_game_state == BATTLE_GAME_STATE_WAIT_DIRECTION
        || g_battle_game_state == BATTLE_GAME_STATE_PRE_ATTACK_ANIMATION) {
        unit = battle_unit_get_source();
        height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        if (unit->camera_relative_x >= 0xB1 && unit->camera_relative_x < 0x150
            && unit->camera_relative_y - height >= 0x51 && unit->camera_relative_y < 0xA0) {
            main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (unit->camera_relative_x >= 0xA1 && unit->camera_relative_x < 0x160
            && unit->camera_relative_y - height >= 0x41 && unit->camera_relative_y < 0xB0) {
            camera_calculate_acting_unit_vector_normal();
        } else {
            camera_shift_acting_unit_vector();
        }
        camera_step_real_coords_toward_target(unit);
        return;
    }
    if (g_battle_game_state >= BATTLE_GAME_STATE_ACTION_ANNOUNCEMENT) {
        main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
        return;
    }
    if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_map_pan_enabled == 0) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_battle_cursor_screen_x < 0xB1 || g_battle_cursor_screen_x >= 0x150
            || g_battle_cursor_screen_y - height < 0x51 || g_battle_cursor_screen_y >= 0xA0) {
            if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
                && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
                camera_step_focus_toward_cursor_tile(camera_calculate_cursor_tile_vector_normal());
            } else {
                /* The scaling routine passes the normal routine's result through. */
                camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
            }
            return;
        }
        main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
    }
}

/* Refresh the cursor-tile camera vector while the map camera is idle.
 *
 * The cursor unit's sprite height (scaled by g_camera_zoom.vy, 4.12 fixed point)
 * offsets the cursor's screen position (g_battle_cursor_screen_x, g_battle_cursor_screen_y). Inside the
 * central window the vector is cleared; nearer the edge the unit tile normal
 * is used as is, otherwise doubled. The tile height left in $v0 by either
 * vector routine is passed on to camera_step_focus_toward_cursor_tile. */
void camera_update_cursor_tile_vector(void) {
    battle_unit_t* unit;
    s32 height;

    if (g_camera_zoom_action == CAMERA_STEP_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_map_pan_enabled == 0) {
        unit = battle_unit_find_selectable_at_tile(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
        height = 0;
        if (unit != 0) {
            height = (g_battle_gfx_spritesheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_battle_cursor_screen_x >= 0xB1 && g_battle_cursor_screen_x < 0x150
            && g_battle_cursor_screen_y - height >= 0x51 && g_battle_cursor_screen_y < 0xA0) {
            main_util_set_vector(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (g_battle_cursor_screen_x >= 0xA1 && g_battle_cursor_screen_x < 0x160
            && g_battle_cursor_screen_y - height >= 0x41 && g_battle_cursor_screen_y < 0xB0) {
            camera_step_focus_toward_cursor_tile(camera_calculate_cursor_tile_vector_normal());
        } else {
            /* The scaling routine passes through the normal routine's height bound. */
            camera_step_focus_toward_cursor_tile(camera_scale_cursor_tile_vector());
        }
    }
}
