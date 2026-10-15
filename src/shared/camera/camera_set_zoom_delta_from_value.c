#include "fft/battle_camera.h"
#include "fft/main.h"
#include "psx/libgpu.h"

/* Starts a linear change of all three zoom scales to the one value
 * *destination_value over frame_count frames (divided by the animation speed),
 * applied by camera_update_zoom_animation. No retail code calls it. */
void camera_set_zoom_delta_from_value(const s32* destination_value, s32 frame_count) {
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
