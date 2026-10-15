/* Rotation save/restore, get/set and the timed angle moves. */

#include "fft/battle_camera.h"
#include "fft/main.h"
#include "psx/libgpu.h"

void camera_save_rotation(void) {
    copyVector(&g_camera_rotation_backup, &g_camera_render_state);
}

void camera_restore_rotation(void) {
    copyVector(&g_camera_render_state, &g_camera_rotation_backup);
}

SVECTOR* camera_get_rotation(void) {
    return &g_camera_render_state;
}

void camera_set_rotation(const SVECTOR* rotation) {
    copyVector(&g_camera_render_state, rotation);
}

/* Starts a linear turn of all three camera angles to the one value
 * *destination_value over frame_count frames (divided by the animation speed),
 * applied by camera_update_rotation_animation. No retail code calls it. */
void camera_set_rotation_delta_from_value(const s16* destination_value, s32 frame_count) {
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
void camera_set_rotation_delta_from_values(const s16* values, s32 frame_count) {
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
void camera_update_rotation_animation(void) {
    if (g_camera_rotation_countdown != 0) {
        g_camera_rotation_countdown--;
        g_camera_render_state.vx += g_camera_rotation_delta.vx;
        g_camera_render_state.vy += g_camera_rotation_delta.vy;
        g_camera_render_state.vz += g_camera_rotation_delta.vz;
    }
}
