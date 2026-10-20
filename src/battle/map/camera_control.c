/* Battle camera input, focus tracking and unit movement updates. */

#include "fft/battle_map.h"
#include "fft/battle_state.h"
#include "fft/camera.h"
#include "fft/cursor.h"
#include "fft/input.h"
#include "fft/move_step.h"
#include "fft/sound.h"
#include "fft/sprite_sheet.h"
#include "fft/system.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "fft/vector.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Camera step rates and cursor repeat state. */
extern s32 g_camera_rotation_increment;
extern s32 g_camera_yaw_target;

/* Camera yaw rounded down to 0x200 (yaw & 0xfe00), stored as each turn's destination is set;
 * nothing reads it. */
extern s32 g_camera_previous_yaw;
extern s32 g_camera_tilt_increment;
extern s32 g_camera_zoom_increment;

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
        switch (cursor_input_direction_get(CAMERA_INPUT_HELD)) {
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
static void rotation_left_destination_set(s32 target_angle) {
    g_camera_previous_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle + 0x200;
    }
}

/* Start a left turn to target_angle from the slowest speed. */
static void rotation_left_start(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_LEFT;
    g_camera_rotation_increment = 1;
    rotation_left_destination_set(target_angle);
}

/* Set the destination of a right turn (rising yaw) and record the yaw it
 * leaves. A destination on a map axis (a multiple of 0x400) is moved 0x200
 * back, so the camera always comes to rest on a diagonal. */
static void rotation_right_destination_set(s32 target_angle) {
    g_camera_previous_yaw = g_camera_render_state.vy & 0xfe00;
    g_camera_yaw_target = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_camera_yaw_target = target_angle - 0x200;
    }
}

/* Start a right turn to target_angle from the slowest speed. */
static void rotation_right_start(s32 target_angle) {
    g_camera_rotation_action = CAMERA_ROTATION_RIGHT;
    g_camera_rotation_increment = 1;
    rotation_right_destination_set(target_angle);
}

/* Start an automatic turn of angle yaw units (0x400 is a quarter turn;
 * negative turns left) with its sound, unless a rotation is running. */
static void rotation_request(s32 angle) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (angle < 0) {
            rotation_left_start((g_camera_render_state.vy & 0xFE00) + angle);
            sound_free_channel_sfx_play(SOUND_SFX_CAMERA_ROTATE_LEFT);
        } else {
            rotation_right_start((g_camera_render_state.vy & 0xFE00) + angle);
            sound_free_channel_sfx_play(SOUND_SFX_CAMERA_ROTATE_RIGHT);
        }
    }
}

/* L1 starts a quarter turn left and R1 one right, each with its sound, unless
 * a rotation is running. */
void camera_rotation_handle_input(void) {
    if (g_camera_rotation_action == CAMERA_ROTATION_NONE) {
        if (g_input_pressed_buttons & PSX_PAD_L1) {
            rotation_left_start((g_camera_render_state.vy & 0xFE00) - 0x400);
            sound_free_channel_sfx_play(SOUND_SFX_CAMERA_ROTATE_LEFT);
        }
        if (g_input_pressed_buttons & PSX_PAD_R1) {
            rotation_right_start((g_camera_render_state.vy & 0xFE00) + 0x400);
            sound_free_channel_sfx_play(SOUND_SFX_CAMERA_ROTATE_RIGHT);
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
                if (g_battle_state != BATTLE_STATE_EVENT) {
                    rotation_left_destination_set((angle & 0xFE0) - 0x400);
                }
            } else {
                g_camera_rotation_action = CAMERA_ROTATION_NONE;
                sound_sfx_stop(SOUND_SFX_CAMERA_ROTATE_RIGHT);
                sound_sfx_stop(SOUND_SFX_CAMERA_ROTATE_LEFT);
                sound_free_channel_sfx_play(SOUND_SFX_CAMERA_ROTATE_STOP);
            }
        }
    } else if (g_camera_rotation_action == CAMERA_ROTATION_RIGHT) {
        s16* yaw = &g_camera_render_state.vy;
        *yaw += g_camera_rotation_increment;
        if (*yaw >= g_camera_yaw_target) {
            angle = (u16)g_camera_yaw_target & 0xFFF;
            *yaw = angle;
            if (g_input_previous_buttons & PSX_PAD_R1) {
                if (g_battle_state != BATTLE_STATE_EVENT) {
                    /* Retail continues the right turn through rotate_left. Harmless: the two
                     * differ only for a destination on an axis, which this one never is. */
                    rotation_left_destination_set((angle & 0xFE0) + 0x400);
                }
            } else {
                g_camera_rotation_action = CAMERA_ROTATION_NONE;
                sound_sfx_stop(SOUND_SFX_CAMERA_ROTATE_RIGHT);
                sound_sfx_stop(SOUND_SFX_CAMERA_ROTATE_LEFT);
                sound_free_channel_sfx_play(SOUND_SFX_CAMERA_ROTATE_STOP);
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
 * it; the L2 input goes through zoom_toggle. */
void camera_zoom_start(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_zoom.vx > ONE) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            sound_sfx_play(SOUND_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < ONE) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            sound_sfx_play(SOUND_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    case 1:
        if (g_camera_zoom.vx > 0xC00) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_DECREASE;
            g_camera_zoom_increment = 4;
            sound_sfx_play(SOUND_SFX_CAMERA_ZOOM_OUT);
            return;
        }
        if (g_camera_zoom.vx < 0xC00) {
            g_camera_zoom_target = BATTLE_MAP_ZOOM_OUT;
            g_camera_zoom_action = CAMERA_STEP_INCREASE;
            g_camera_zoom_increment = 4;
            sound_sfx_play(SOUND_SFX_CAMERA_ZOOM_IN);
            return;
        }
        return;
    }
}

/* Toggle the zoom target between BATTLE_MAP_ZOOM_IN and BATTLE_MAP_ZOOM_OUT, starting the
 * matching zoom with its sound; any other target is left alone. The increment
 * restarts at 4 either way. */
static void zoom_toggle(void) {
    if (g_camera_zoom_target == BATTLE_MAP_ZOOM_IN) {
        g_camera_zoom_action = CAMERA_STEP_DECREASE;
        g_camera_zoom_target = BATTLE_MAP_ZOOM_OUT;
        sound_sfx_play(SOUND_SFX_CAMERA_ZOOM_OUT);
    } else if (g_camera_zoom_target == BATTLE_MAP_ZOOM_OUT) {
        g_camera_zoom_target = BATTLE_MAP_ZOOM_IN;
        g_camera_zoom_action = CAMERA_STEP_INCREASE;
        sound_sfx_play(SOUND_SFX_CAMERA_ZOOM_IN);
    }
    g_camera_zoom_increment = 4;
}

/* Toggles the zoom when L2 is newly pressed and no zoom is running. */
void camera_zoom_handle_input(void) {
    if (g_camera_zoom_action == CAMERA_STEP_NONE) {
        if ((g_input_pressed_buttons & PSX_PAD_L2) != 0) {
            zoom_toggle();
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
 * R2 input goes through tilt_toggle. */
void camera_tilt_start(s32 mode) {
    switch (mode) {
    case 0:
        if (g_camera_render_state.vx >= 0x12F) {
            g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = 1;
            sound_sfx_play(SOUND_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x12E) {
            g_camera_tilt_target = BATTLE_MAP_TILT_LOW;
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_camera_tilt_increment = 1;
            sound_sfx_play(SOUND_SFX_CAMERA_TILT);
        }
        break;
    case 1:
        if (g_camera_render_state.vx >= 0x1C1) {
            g_camera_tilt_target = BATTLE_MAP_TILT_HIGH;
            g_camera_tilt_action = CAMERA_STEP_DECREASE;
            g_camera_tilt_increment = 1;
            sound_sfx_play(SOUND_SFX_CAMERA_TILT);
        } else if (g_camera_render_state.vx < 0x1C0) {
            g_camera_tilt_action = CAMERA_STEP_INCREASE;
            g_camera_tilt_target = BATTLE_MAP_TILT_HIGH;
            g_camera_tilt_increment = 1;
            sound_sfx_play(SOUND_SFX_CAMERA_TILT);
        }
        break;
    }
}

/* Toggle the tilt target between low and high, starting the tilt at increment 1
 * with its sound. Dormant targets 3 and 4 advance 3 -> 4 -> LOW; no named writer selects 3. */
static void tilt_toggle(void) {
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
    sound_sfx_play(SOUND_SFX_CAMERA_TILT);
}

/* Toggles the tilt when R2 is newly pressed and no tilt is running. */
void camera_tilt_handle_input(void) {
    if (g_camera_tilt_action == CAMERA_STEP_NONE) {
        if ((g_input_pressed_buttons & PSX_PAD_R2) != 0) {
            tilt_toggle();
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
static void acting_unit_vector_normal_calculate(void) {
    VECTOR delta;
    unit_runtime_t* unit = unit_source_get();

    setVector(&delta, unit->world_coords.vx - g_camera_real_coords.vx / ONE,
        unit->world_coords.vy - g_camera_real_coords.vy / ONE, unit->world_coords.vz - g_camera_real_coords.vz / ONE);
    VectorNormal(&delta, &g_camera_step_vector);
    g_camera_step_vector.vy = g_camera_step_vector.vy << 2;
}

/* The acting-unit camera step vector at double length, used when the unit is
 * outside the wider screen window. */
static void acting_unit_vector_shift(void) {
    acting_unit_vector_normal_calculate();
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
static s32 cursor_tile_vector_normal_calculate(void) {
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
    vector_short_set(&tile_position, g_cursor_x * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2), height,
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
 * cursor_tile_vector_normal_calculate and passes that routine's
 * result through; callers hand it to cursor_focus_step. */
static s32 cursor_tile_vector_scale(void) {
    s32 result = cursor_tile_vector_normal_calculate();

    g_camera_step_vector.vx *= 2;
    g_camera_step_vector.vy *= 2;
    g_camera_step_vector.vz *= 2;
    return result;
}

/* Move one 20.12 fixed-point camera component toward its bound by its per-frame
 * step, without overshooting. The bound is compared against the component's
 * whole-unit part, so the clamp lands exactly on bound * ONE. The test is
 * written once per sign of the step because the overshoot comparison reverses
 * with it. A do/while (0) wrapper changes the compiler's loop weighting and
 * register allocation, so this macro keeps a bare if/else body. */
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
static void cursor_focus_step(s16 height) {
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
static void real_coords_toward_target_step(const unit_runtime_t* target) {
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
 * BATTLE_STATE_ACTION_ANNOUNCEMENT take the idle path of
 * camera_cursor_focus_update, and that state and those above it
 * clear the vector.
 *
 * The idle path is camera_cursor_focus_update verbatim. The four
 * vector_long_set calls are one tail-merged block in the target, placed
 * last so it falls into the epilogue, so the idle path's window test is
 * spelled inverted with the clear-vector call as the function's last
 * statement. Written the natural way, the merged block lands mid-function
 * with an extra `j`. */
void camera_scene_focus_update(void) {
    unit_runtime_t* unit;
    s32 height;

    if (g_battle_state == BATTLE_STATE_POST_MOVE_EVENTS) {
        unit = unit_find_selectable_at_tile(g_cursor_x, g_cursor_y, g_cursor_layer);
        height = 0;
        if (unit != 0) {
            height = (g_sprite_sheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        }
        if (g_cursor_screen_x >= 0xF9 && g_cursor_screen_x < 0x108 && g_cursor_screen_y - height >= 0xA1
            && g_cursor_screen_y < 0xB0) {
            vector_long_set(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        /* The scaling routine passes through the normal routine's height bound. */
        cursor_focus_step(cursor_tile_vector_scale());
        return;
    }
    if (g_battle_state == BATTLE_STATE_UNIT_MOVING || g_battle_state == BATTLE_STATE_WAIT_DIRECTION
        || g_battle_state == BATTLE_STATE_PRE_ATTACK_ANIMATION) {
        unit = unit_source_get();
        height = (g_sprite_sheet_data[unit->spritesheet_id].graphic_height >> 1) * g_camera_zoom.vy / ONE;
        if (unit->camera_relative_x >= 0xB1 && unit->camera_relative_x < 0x150
            && unit->camera_relative_y - height >= 0x51 && unit->camera_relative_y < 0xA0) {
            vector_long_set(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (unit->camera_relative_x >= 0xA1 && unit->camera_relative_x < 0x160
            && unit->camera_relative_y - height >= 0x41 && unit->camera_relative_y < 0xB0) {
            acting_unit_vector_normal_calculate();
        } else {
            acting_unit_vector_shift();
        }
        real_coords_toward_target_step(unit);
        return;
    }
    if (g_battle_state >= BATTLE_STATE_ACTION_ANNOUNCEMENT) {
        vector_long_set(&g_camera_step_vector, 0, 0, 0);
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
                cursor_focus_step(cursor_tile_vector_normal_calculate());
            } else {
                /* The scaling routine passes the normal routine's result through. */
                cursor_focus_step(cursor_tile_vector_scale());
            }
            return;
        }
        vector_long_set(&g_camera_step_vector, 0, 0, 0);
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
 * cursor_focus_step. */
void camera_cursor_focus_update(void) {
    unit_runtime_t* unit;
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
            vector_long_set(&g_camera_step_vector, 0, 0, 0);
            return;
        }
        if (g_cursor_screen_x >= 0xA1 && g_cursor_screen_x < 0x160 && g_cursor_screen_y - height >= 0x41
            && g_cursor_screen_y < 0xB0) {
            cursor_focus_step(cursor_tile_vector_normal_calculate());
        } else {
            /* The scaling routine passes through the normal routine's height bound. */
            cursor_focus_step(cursor_tile_vector_scale());
        }
    }
}

/* Advance the movement of each runtime unit that has a path. Runs every frame of event mode and of
 * the change-map jump-in state. */
void move_units_update(void) {
    s32 i;
    unit_runtime_t* unit;

    for (i = 0; i < UNIT_RUNTIME_SLOT_COUNT; i++) {
        unit = unit_runtime_get(i);
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
void camera_unit_visibility_rotate(const unit_runtime_t* unit) {
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
                rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    rotation_request(0x800);
                }
            }
        }
        break;
    case 1:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                    rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    rotation_request(0x800);
                }
            }
        }
        break;
    case 2:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST)) {
                    rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    rotation_request(0x800);
                }
            }
        }
        break;
    case 3:
        if (flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHEAST) {
            if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHEAST)) {
                rotation_request(0x400);
            } else {
                if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_NORTHWEST)) {
                    rotation_request(-0x400);
                } else if (!(flags & BATTLE_MAP_TILE_CAMERA_BLOCK_SOUTHWEST)) {
                    rotation_request(0x800);
                }
            }
        }
        break;
    }
}
