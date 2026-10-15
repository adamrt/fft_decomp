#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Starts a linear move of all three screen-offset components to the one value
 * *destination_value over frame_count frames (divided by the animation speed).
 * camera_update_screen_offset_animation applies it.
 *
 * The per-frame step is stored in g_camera_real_coord_delta, the vector
 * the camera-position move also uses. No retail code calls it. */
void camera_start_screen_offset_move_uniform(const s32* destination_value, s32 frame_count) {
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
