#include "fft/battle.h"

void battle_camera_set_rotation_delta_from_values(const s16* values, s32 frame_count) {
    s32 animation_frames;
    s32 value_x;

    animation_frames = (frame_count & 0xffff) / g_animation_speed;
    value_x = values[0];
    g_battle_camera_rotation_countdown = animation_frames;
    animation_frames = g_battle_camera_rotation_countdown;
    g_battle_camera_rotation_delta.vx = value_x / animation_frames;
    g_battle_camera_rotation_delta.vy = values[1] / animation_frames;
    g_battle_camera_rotation_delta.vz = values[2] / animation_frames;
}
