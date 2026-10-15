#include "fft/battle_camera.h"
#include "fft/main.h"
#include "psx/libgpu.h"

/* Starts a linear move of the camera position by the vector `values` (20.12) over
 * frame_count frames (divided by the animation speed). No retail code calls it. */
void camera_set_real_coord_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    g_camera_real_coord_countdown = animation_frames;
    animation_frames = g_camera_real_coord_countdown;
    setVector(&g_camera_real_coord_delta, values->vx / animation_frames, values->vy / animation_frames,
        values->vz / animation_frames);
}
