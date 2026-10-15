#include "fft/battle_camera.h"
#include "fft/main.h"
#include "psx/libgpu.h"

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
