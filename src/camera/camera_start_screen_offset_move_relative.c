#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Starts a linear move of the screen offset by the vector `values` over
 * frame_count frames (divided by the animation speed).
 *
 * The per-frame step is stored in g_camera_real_coord_delta, the vector
 * the camera-position move also uses. No retail code calls it. */
void camera_start_screen_offset_move_relative(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_main_system_animation_speed;
    setVector(&g_camera_screen_offset_fixed, g_camera_current_screen_offset.vx * ONE,
        g_camera_current_screen_offset.vy * ONE, g_camera_current_screen_offset.vz * ONE);
    g_camera_screen_offset_countdown = animation_frames;
    animation_frames = g_camera_screen_offset_countdown;
    setVector(&g_camera_real_coord_delta, (values->vx * ONE) / animation_frames, (values->vy * ONE) / animation_frames,
        (values->vz * ONE) / animation_frames);
}
