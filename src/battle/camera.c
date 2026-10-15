/* The battle camera: real-coordinate and zoom deltas (each written twice in the original), screen offset, zoom and
 * rotation. Both edges are cuts chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/event_attack.h"
#include "fft/main.h"
#include "fft/map.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Saves the camera position for camera_restore_current_real_coords (effect
 * script opcodes 09 and 0a). */
void battle_camera_save_current_real_coords(void) {
    copyVector(&g_camera_saved_real_coords, &g_camera_current_real_coords);
}

void battle_camera_restore_current_real_coords(void) {
    copyVector(&g_camera_current_real_coords, &g_camera_saved_real_coords);
}

VECTOR* battle_camera_get_current_real_coords(void) {
    return &g_camera_current_real_coords;
}

void battle_camera_set_current_real_coords(const VECTOR* coords) {
    copyVector(&g_camera_current_real_coords, coords);
}

/* Starts a linear move of the camera position to `destination` (20.12) over
 * frame_count frames (divided by the animation speed), applied by
 * camera_update_real_coord_animation. No retail code calls it. */
void battle_camera_set_real_coord_delta_from_destination(const VECTOR* destination, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    g_camera_real_coord_countdown = animation_frames;
    animation_frames = g_camera_real_coord_countdown;
    setVector(&g_camera_real_coord_delta, (destination->vx - g_camera_current_real_coords.vx) / animation_frames,
        (destination->vy - g_camera_current_real_coords.vy) / animation_frames,
        (destination->vz - g_camera_current_real_coords.vz) / animation_frames);
}

/* Starts a linear move of the camera position by the vector `values` (20.12) over
 * frame_count frames (divided by the animation speed). No retail code calls it. */
void battle_camera_set_real_coord_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    g_camera_real_coord_countdown = animation_frames;
    animation_frames = g_camera_real_coord_countdown;
    setVector(&g_camera_real_coord_delta, values->vx / animation_frames, values->vy / animation_frames,
        values->vz / animation_frames);
}

/* Advances the camera-position move by one frame while its countdown runs. Only
 * the uncalled camera_set_real_coord_delta_* routines start the countdown,
 * so nothing moves in retail. */
void battle_camera_update_real_coord_animation(void) {
    if (g_camera_real_coord_countdown != 0) {
        g_camera_real_coord_countdown--;
        g_camera_current_real_coords.vx += g_camera_real_coord_delta.vx;
        g_camera_current_real_coords.vy += g_camera_real_coord_delta.vy;
        g_camera_current_real_coords.vz += g_camera_real_coord_delta.vz;
    }
}

void battle_camera_save_screen_offset(void) {
    copyVector(&g_camera_saved_screen_offset, &g_camera_current_screen_offset);
}

/* No retail code calls it. */
void battle_camera_restore_screen_offset(void) {
    copyVector(&g_camera_current_screen_offset, &g_camera_saved_screen_offset);
}

/* No retail code calls it. */
VECTOR* battle_camera_get_screen_offset(void) {
    return &g_camera_current_screen_offset;
}

/* No retail code calls it. */
void battle_camera_set_screen_offset(const VECTOR* coords) {
    copyVector(&g_camera_current_screen_offset, coords);
}

/* Starts a linear move of all three screen-offset components to the one value
 * *destination_value over frame_count frames (divided by the animation speed).
 * camera_update_screen_offset_animation applies it.
 *
 * The per-frame step is stored in g_camera_real_coord_delta, the vector
 * the camera-position move also uses. No retail code calls it. */
void battle_camera_start_screen_offset_move_uniform(const s32* destination_value, s32 frame_count) {
    s32 animation_frames;
    s32 current_x;
    s32 current_z;
    s32 current_y;
    s32 delta_x;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    current_x = g_camera_current_screen_offset.vx;
    current_z = g_camera_current_screen_offset.vy;
    current_y = g_camera_current_screen_offset.vz;
    setVector(&g_camera_screen_offset_fixed, current_x * ONE, current_z * ONE, current_y * ONE);
    delta_x = *destination_value - current_x;
    g_camera_screen_offset_countdown = animation_frames;
    animation_frames = g_camera_screen_offset_countdown;
    setVector(&g_camera_real_coord_delta, (delta_x * ONE) / animation_frames,
        ((*destination_value - current_z) * ONE) / animation_frames,
        ((*destination_value - current_y) * ONE) / animation_frames);
}

/* Starts a linear move of the screen offset by the vector `values` over
 * frame_count frames (divided by the animation speed).
 *
 * The per-frame step is stored in g_camera_real_coord_delta, the vector
 * the camera-position move also uses. No retail code calls it. */
void battle_camera_start_screen_offset_move_relative(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    setVector(&g_camera_screen_offset_fixed, g_camera_current_screen_offset.vx * ONE,
        g_camera_current_screen_offset.vy * ONE, g_camera_current_screen_offset.vz * ONE);
    g_camera_screen_offset_countdown = animation_frames;
    animation_frames = g_camera_screen_offset_countdown;
    setVector(&g_camera_real_coord_delta, (values->vx * ONE) / animation_frames, (values->vy * ONE) / animation_frames,
        (values->vz * ONE) / animation_frames);
}

/* Advances the screen-offset move by one frame while its countdown runs. Only the
 * uncalled camera_set_offset_screen_coord_delta_* routines start the
 * countdown, so nothing moves in retail. */
void battle_camera_update_screen_offset_animation(void) {
    s32 fixed_x;
    s32 fixed_z;
    s32 fixed_y;

    if (g_camera_screen_offset_countdown != 0) {
        fixed_x = g_camera_screen_offset_fixed.vx + g_camera_real_coord_delta.vx;
        g_camera_screen_offset_fixed.vx = fixed_x;
        fixed_z = g_camera_screen_offset_fixed.vy + g_camera_real_coord_delta.vy;
        fixed_y = g_camera_screen_offset_fixed.vz + g_camera_real_coord_delta.vz;
        g_camera_screen_offset_fixed.vy = fixed_z;
        g_camera_screen_offset_fixed.vz = fixed_y;
        setVector(&g_camera_current_screen_offset, fixed_x / ONE, fixed_z / ONE, fixed_y / ONE);
        g_camera_screen_offset_countdown--;
    }
}

/* Saves the camera zoom for camera_restore_zoom (effect script opcodes 0f
 * and 24). */
void battle_camera_save_zoom(void) {
    copyVector(&g_camera_zoom_backup, &g_camera_zoom);
}

void battle_camera_restore_zoom(void) {
    copyVector(&g_camera_zoom, &g_camera_zoom_backup);
}

VECTOR* battle_camera_get_zoom(void) {
    return &g_camera_zoom;
}

void battle_camera_set_zoom(const VECTOR* zoom) {
    copyVector(&g_camera_zoom, zoom);
}

/* Starts a linear change of all three zoom scales to the one value
 * *destination_value over frame_count frames (divided by the animation speed),
 * applied by camera_update_zoom_animation. No retail code calls it. */
void battle_camera_set_zoom_delta_from_value(const s32* destination_value, s32 frame_count) {
    s32 animation_frames;
    s32 delta_x;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    delta_x = *destination_value - g_camera_zoom.vx;
    g_camera_zoom_countdown = animation_frames;
    animation_frames = g_camera_zoom_countdown;
    setVector(&g_camera_zoom_delta, delta_x / animation_frames,
        (*destination_value - g_camera_zoom.vy) / animation_frames,
        (*destination_value - g_camera_zoom.vz) / animation_frames);
}

/* The original wrote the body of battle_camera_set_real_coord_delta_from_values a second time as
 * battle_camera_set_zoom_delta_from_values. */
/* Starts a linear move of the camera position by the vector `values` (20.12) over
 * frame_count frames (divided by the animation speed). No retail code calls it. */
void battle_camera_set_zoom_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    g_camera_zoom_countdown = animation_frames;
    animation_frames = g_camera_zoom_countdown;
    setVector(&g_camera_zoom_delta, values->vx / animation_frames, values->vy / animation_frames,
        values->vz / animation_frames);
}

/* The original wrote the body of battle_camera_update_real_coord_animation a second time as
 * battle_camera_update_zoom_animation. */
/* Advances the camera-position move by one frame while its countdown runs. Only
 * the uncalled camera_set_real_coord_delta_* routines start the countdown,
 * so nothing moves in retail. */
void battle_camera_update_zoom_animation(void) {
    if (g_camera_zoom_countdown != 0) {
        g_camera_zoom_countdown--;
        g_camera_zoom.vx += g_camera_zoom_delta.vx;
        g_camera_zoom.vy += g_camera_zoom_delta.vy;
        g_camera_zoom.vz += g_camera_zoom_delta.vz;
    }
}

void battle_camera_save_rotation(void) {
    copyVector(&g_camera_rotation_backup, &g_camera_render_state);
}

void battle_camera_restore_rotation(void) {
    copyVector(&g_camera_render_state, &g_camera_rotation_backup);
}

SVECTOR* battle_camera_get_rotation(void) {
    return &g_camera_render_state;
}

void battle_camera_set_rotation(const SVECTOR* rotation) {
    copyVector(&g_camera_render_state, rotation);
}

/* Starts a linear turn of all three camera angles to the one value
 * *destination_value over frame_count frames (divided by the animation speed),
 * applied by camera_update_rotation_animation. No retail code calls it. */
void battle_camera_set_rotation_delta_from_value(const s16* destination_value, s32 frame_count) {
    s32 animation_frames;
    s32 delta_x;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    delta_x = *destination_value - g_camera_render_state.vx;
    g_camera_rotation_countdown = animation_frames;
    animation_frames = g_camera_rotation_countdown;
    setVector(&g_camera_rotation_delta, delta_x / animation_frames,
        (*destination_value - g_camera_render_state.vy) / animation_frames,
        (*destination_value - g_camera_render_state.vz) / animation_frames);
}

/* Starts a linear turn of the camera angles by values[0..2] over frame_count
 * frames (divided by the animation speed). No retail code calls it. */
void battle_camera_set_rotation_delta_from_values(const s16* values, s32 frame_count) {
    s32 animation_frames;
    s32 value_x;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    value_x = values[0];
    g_camera_rotation_countdown = animation_frames;
    animation_frames = g_camera_rotation_countdown;
    setVector(&g_camera_rotation_delta, value_x / animation_frames, values[1] / animation_frames,
        values[2] / animation_frames);
}

/* Advances the camera-angle move by one frame while its countdown runs. Only the
 * uncalled camera_set_rotation_delta_* routines start the countdown, so
 * nothing moves in retail. */
void battle_camera_update_rotation_animation(void) {
    if (g_camera_rotation_countdown != 0) {
        g_camera_rotation_countdown--;
        g_camera_render_state.vx += g_camera_rotation_delta.vx;
        g_camera_render_state.vy += g_camera_rotation_delta.vy;
        g_camera_render_state.vz += g_camera_rotation_delta.vz;
    }
}
