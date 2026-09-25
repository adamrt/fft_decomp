#include "fft/battle.h"

void battle_camera_set_rotation_delta_from_value(const s16* destination_value, s32 frame_count) {
    s32 animation_frames;
    s32 delta_x;

    animation_frames = (frame_count & 0xffff) / g_animation_speed;
    delta_x = *destination_value - g_battle_camera_render_state.vx;
    g_battle_camera_rotation_countdown = animation_frames;
    animation_frames = g_battle_camera_rotation_countdown;
    g_battle_camera_rotation_delta.vx = delta_x / animation_frames;
    g_battle_camera_rotation_delta.vy = (*destination_value - g_battle_camera_render_state.vy) / animation_frames;
    g_battle_camera_rotation_delta.vz = (*destination_value - g_battle_camera_render_state.vz) / animation_frames;
}
