/* The battle camera, with saved state, deltas and animation steps for real coordinates, screen offset, zoom and
 * rotation (the real-coordinate and zoom delta setters are written twice in the original). */

#include "fft/camera.h"
#include "fft/system.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Interpolation state and saved values for camera restore commands. */
extern u16 g_camera_real_coords_countdown;
extern VECTOR g_camera_real_coords_delta;
extern VECTOR g_camera_screen_offset_saved;
extern VECTOR g_camera_screen_offset_fixed;
extern SVECTOR g_camera_rotation_saved;
extern VECTOR g_camera_zoom_saved;

static VECTOR g_camera_real_coords_saved = { 0 };
static SVECTOR g_camera_rotation_delta = { 0 };
static VECTOR g_camera_zoom_delta = { 0 };

/* Saves the camera position for camera_real_coords_restore (effect
 * script opcodes 09 and 0a). */
void camera_real_coords_save(void) {
    copyVector(&g_camera_real_coords_saved, &g_camera_real_coords);
}

/* Restores the camera position kept by camera_real_coords_save. */
void camera_real_coords_restore(void) {
    copyVector(&g_camera_real_coords, &g_camera_real_coords_saved);
}

VECTOR* camera_real_coords_get(void) {
    return &g_camera_real_coords;
}

/* Sets the camera position (20.12 real coordinates) at once. */
void camera_real_coords_set(const VECTOR* coords) {
    copyVector(&g_camera_real_coords, coords);
}

/* Starts a linear move of the camera position to `destination` (20.12) over
 * frame_count frames (divided by the vsync interval), applied by
 * camera_real_coords_update_animation. No retail code calls it.
 *
 * Like every move starter here, it divides by the frame count as read back from
 * the 16-bit countdown, so the step always matches the frames actually run. */
void camera_real_coords_set_delta_from_destination(const VECTOR* destination, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;

    g_camera_real_coords_countdown = animation_frames;
    animation_frames = g_camera_real_coords_countdown;
    setVector(&g_camera_real_coords_delta, (destination->vx - g_camera_real_coords.vx) / animation_frames,
        (destination->vy - g_camera_real_coords.vy) / animation_frames,
        (destination->vz - g_camera_real_coords.vz) / animation_frames);
}

/* Starts a linear move of the camera position by the vector `values` (20.12) over
 * frame_count frames (divided by the vsync interval). No retail code calls it. */
void camera_real_coords_set_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;

    g_camera_real_coords_countdown = animation_frames;
    animation_frames = g_camera_real_coords_countdown;
    setVector(&g_camera_real_coords_delta, values->vx / animation_frames, values->vy / animation_frames,
        values->vz / animation_frames);
}

/* Advances the camera-position move by one frame while its countdown runs. Only
 * the uncalled camera_real_coords_set_delta_* routines start the countdown,
 * so nothing moves in retail. */
void camera_real_coords_update_animation(void) {
    if (g_camera_real_coords_countdown != 0) {
        g_camera_real_coords_countdown--;
        g_camera_real_coords.vx += g_camera_real_coords_delta.vx;
        g_camera_real_coords.vy += g_camera_real_coords_delta.vy;
        g_camera_real_coords.vz += g_camera_real_coords_delta.vz;
    }
}

/* No retail code calls it. */
void camera_screen_offset_save(void) {
    copyVector(&g_camera_screen_offset_saved, &g_camera_screen_offset);
}

/* No retail code calls it. */
void camera_screen_offset_restore(void) {
    copyVector(&g_camera_screen_offset, &g_camera_screen_offset_saved);
}

/* No retail code calls it. */
VECTOR* camera_screen_offset_get(void) {
    return &g_camera_screen_offset;
}

/* No retail code calls it. */
void camera_screen_offset_set(const VECTOR* coords) {
    copyVector(&g_camera_screen_offset, coords);
}

/* Starts a linear move of all three screen-offset components to the one value
 * *destination_value over frame_count frames (divided by the vsync interval).
 * camera_screen_offset_update_animation applies it.
 *
 * The per-frame step is stored in g_camera_real_coords_delta, the vector
 * the camera-position move also uses. No retail code calls it. */
void camera_screen_offset_start_move_uniform(const s32* destination_value, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;
    s32 current_x = g_camera_screen_offset.vx;
    s32 current_y = g_camera_screen_offset.vy;
    s32 current_z = g_camera_screen_offset.vz;
    s32 delta_x;

    /* The move runs in 20.12 so fractional steps accumulate. */
    setVector(&g_camera_screen_offset_fixed, current_x * ONE, current_y * ONE, current_z * ONE);
    delta_x = *destination_value - current_x;
    g_camera_screen_offset_countdown = animation_frames;
    animation_frames = g_camera_screen_offset_countdown;
    setVector(&g_camera_real_coords_delta, (delta_x * ONE) / animation_frames,
        ((*destination_value - current_y) * ONE) / animation_frames,
        ((*destination_value - current_z) * ONE) / animation_frames);
}

/* Starts a linear move of the screen offset by the vector `values` over
 * frame_count frames (divided by the vsync interval).
 *
 * The per-frame step is stored in g_camera_real_coords_delta, the vector
 * the camera-position move also uses. No retail code calls it. */
void camera_screen_offset_start_move_relative(const VECTOR* values, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;

    setVector(&g_camera_screen_offset_fixed, g_camera_screen_offset.vx * ONE, g_camera_screen_offset.vy * ONE,
        g_camera_screen_offset.vz * ONE);
    g_camera_screen_offset_countdown = animation_frames;
    animation_frames = g_camera_screen_offset_countdown;
    setVector(&g_camera_real_coords_delta, (values->vx * ONE) / animation_frames, (values->vy * ONE) / animation_frames,
        (values->vz * ONE) / animation_frames);
}

/* Advances the screen-offset move by one frame while its countdown runs. Only the
 * uncalled camera_screen_offset_start_move_uniform and _relative routines
 * start the countdown, so nothing moves in retail. */
void camera_screen_offset_update_animation(void) {
    s32 fixed_x;
    s32 fixed_y;
    s32 fixed_z;

    if (g_camera_screen_offset_countdown != 0) {
        fixed_x = g_camera_screen_offset_fixed.vx + g_camera_real_coords_delta.vx;
        g_camera_screen_offset_fixed.vx = fixed_x;
        fixed_y = g_camera_screen_offset_fixed.vy + g_camera_real_coords_delta.vy;
        fixed_z = g_camera_screen_offset_fixed.vz + g_camera_real_coords_delta.vz;
        g_camera_screen_offset_fixed.vy = fixed_y;
        g_camera_screen_offset_fixed.vz = fixed_z;
        setVector(&g_camera_screen_offset, fixed_x / ONE, fixed_y / ONE, fixed_z / ONE);
        g_camera_screen_offset_countdown--;
    }
}

/* Saves the camera zoom for camera_zoom_restore (effect script opcodes 0f
 * and 24). */
void camera_zoom_save(void) {
    copyVector(&g_camera_zoom_saved, &g_camera_zoom);
}

/* Restores the camera zoom kept by camera_zoom_save. */
void camera_zoom_restore(void) {
    copyVector(&g_camera_zoom, &g_camera_zoom_saved);
}

VECTOR* camera_zoom_get(void) {
    return &g_camera_zoom;
}

/* Sets the camera's three zoom scales at once. */
void camera_zoom_set(const VECTOR* zoom) {
    copyVector(&g_camera_zoom, zoom);
}

/* Starts a linear change of all three zoom scales to the one value
 * *destination_value over frame_count frames (divided by the vsync interval),
 * applied by camera_zoom_update_animation. No retail code calls it. */
void camera_zoom_set_delta_from_value(const s32* destination_value, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;
    s32 delta_x = *destination_value - g_camera_zoom.vx;

    g_camera_zoom_countdown = animation_frames;
    animation_frames = g_camera_zoom_countdown;
    setVector(&g_camera_zoom_delta, delta_x / animation_frames,
        (*destination_value - g_camera_zoom.vy) / animation_frames,
        (*destination_value - g_camera_zoom.vz) / animation_frames);
}

/* Starts a linear change of the zoom scales by the vector `values` over
 * frame_count frames (divided by the vsync interval), applied by
 * camera_zoom_update_animation. No retail code calls it. The body repeats
 * camera_real_coords_set_delta_from_values with the zoom globals. */
void camera_zoom_set_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;

    g_camera_zoom_countdown = animation_frames;
    animation_frames = g_camera_zoom_countdown;
    setVector(&g_camera_zoom_delta, values->vx / animation_frames, values->vy / animation_frames,
        values->vz / animation_frames);
}

/* Advances the zoom change by one frame while its countdown runs. Only the
 * uncalled camera_zoom_set_delta_* routines start the countdown, so the
 * zoom never animates here in retail. The body repeats
 * camera_real_coords_update_animation with the zoom globals. */
void camera_zoom_update_animation(void) {
    if (g_camera_zoom_countdown != 0) {
        g_camera_zoom_countdown--;
        g_camera_zoom.vx += g_camera_zoom_delta.vx;
        g_camera_zoom.vy += g_camera_zoom_delta.vy;
        g_camera_zoom.vz += g_camera_zoom_delta.vz;
    }
}

/* Saves the camera rotation for camera_rotation_restore (effect script opcodes 0c and 0d). */
void camera_rotation_save(void) {
    copyVector(&g_camera_rotation_saved, &g_camera_render_state);
}

/* Restores the camera rotation kept by camera_rotation_save. */
void camera_rotation_restore(void) {
    copyVector(&g_camera_render_state, &g_camera_rotation_saved);
}

SVECTOR* camera_rotation_get(void) {
    return &g_camera_render_state;
}

/* Sets the camera's three rotation angles at once. */
void camera_rotation_set(const SVECTOR* rotation) {
    copyVector(&g_camera_render_state, rotation);
}

/* Starts a linear turn of all three camera angles to the one value
 * *destination_value over frame_count frames (divided by the vsync interval),
 * applied by camera_rotation_update_animation. No retail code calls it. */
void camera_rotation_set_delta_from_value(const s16* destination_value, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;
    s32 delta_x = *destination_value - g_camera_render_state.vx;

    g_camera_rotation_countdown = animation_frames;
    animation_frames = g_camera_rotation_countdown;
    setVector(&g_camera_rotation_delta, delta_x / animation_frames,
        (*destination_value - g_camera_render_state.vy) / animation_frames,
        (*destination_value - g_camera_render_state.vz) / animation_frames);
}

/* Starts a linear turn of the camera angles by values[0..2] over frame_count
 * frames (divided by the vsync interval). No retail code calls it. */
void camera_rotation_set_delta_from_values(const s16* values, s32 frame_count) {
    s32 animation_frames = (frame_count & 0xffff) / g_system_vsync_interval;
    s32 value_x = values[0];

    g_camera_rotation_countdown = animation_frames;
    animation_frames = g_camera_rotation_countdown;
    setVector(&g_camera_rotation_delta, value_x / animation_frames, values[1] / animation_frames,
        values[2] / animation_frames);
}

/* Advances the camera-angle move by one frame while its countdown runs. Only the
 * uncalled camera_rotation_set_delta_* routines start the countdown, so
 * nothing moves in retail. */
void camera_rotation_update_animation(void) {
    if (g_camera_rotation_countdown != 0) {
        g_camera_rotation_countdown--;
        g_camera_render_state.vx += g_camera_rotation_delta.vx;
        g_camera_render_state.vy += g_camera_rotation_delta.vy;
        g_camera_render_state.vz += g_camera_rotation_delta.vz;
    }
}
