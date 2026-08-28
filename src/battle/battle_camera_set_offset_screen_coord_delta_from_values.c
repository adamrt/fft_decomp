#include "fft/battle.h"
#include "fft/main_runtime.h"

void battle_camera_set_offset_screen_coord_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_animation_speed;
    g_battle_camera_offset_screen_coords_fixed.vx = g_battle_offset_screen_coords.vx << 12;
    g_battle_camera_offset_screen_coords_fixed.vy = g_battle_offset_screen_coords.vy << 12;
    g_battle_camera_offset_screen_coords_fixed.vz = g_battle_offset_screen_coords.vz << 12;
    g_battle_camera_offset_screen_coord_countdown = animation_frames;
    animation_frames = g_battle_camera_offset_screen_coord_countdown;
    g_battle_camera_real_coord_delta.vx = (values->vx << 12) / animation_frames;
    g_battle_camera_real_coord_delta.vy = (values->vy << 12) / animation_frames;
    g_battle_camera_real_coord_delta.vz = (values->vz << 12) / animation_frames;
}
