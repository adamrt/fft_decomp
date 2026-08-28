#include "fft/battle.h"
#include "fft/main_runtime.h"

void battle_camera_set_real_coord_delta_from_destination(const VECTOR* destination, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_animation_speed;
    g_battle_camera_real_coord_countdown = animation_frames;
    animation_frames = g_battle_camera_real_coord_countdown;
    g_battle_camera_real_coord_delta.vx = (destination->vx - g_battle_camera_current_real_coords.vx) / animation_frames;
    g_battle_camera_real_coord_delta.vy = (destination->vy - g_battle_camera_current_real_coords.vy) / animation_frames;
    g_battle_camera_real_coord_delta.vz = (destination->vz - g_battle_camera_current_real_coords.vz) / animation_frames;
}
