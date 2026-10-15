#include "fft/battle_camera.h"
#include "fft/main.h"
#include "psx/libgpu.h"

/* Starts a linear move of the camera position to `destination` (20.12) over
 * frame_count frames (divided by the animation speed), applied by
 * camera_update_real_coord_animation. No retail code calls it. */
void camera_set_real_coord_delta_from_destination(const VECTOR* destination, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    g_camera_real_coord_countdown = animation_frames;
    animation_frames = g_camera_real_coord_countdown;
    setVector(&g_camera_real_coord_delta, (destination->vx - g_camera_current_real_coords.vx) / animation_frames,
        (destination->vy - g_camera_current_real_coords.vy) / animation_frames,
        (destination->vz - g_camera_current_real_coords.vz) / animation_frames);
}
