#include "fft/battle_camera.h"
#include "fft/main.h"
#include "psx/libgpu.h"

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
