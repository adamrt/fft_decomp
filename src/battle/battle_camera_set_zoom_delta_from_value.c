#include "fft/battle.h"

void battle_camera_set_zoom_delta_from_value(const s32* destination_value, s32 frame_count) {
    s32 animation_frames;
    s32 delta_x;

    animation_frames = (frame_count & 0xffff) / g_animation_speed;
    delta_x = *destination_value - g_battle_camera_zoom.vx;
    g_battle_camera_zoom_countdown = animation_frames;
    animation_frames = g_battle_camera_zoom_countdown;
    g_battle_camera_zoom_delta.vx = delta_x / animation_frames;
    g_battle_camera_zoom_delta.vy = (*destination_value - g_battle_camera_zoom.vy) / animation_frames;
    g_battle_camera_zoom_delta.vz = (*destination_value - g_battle_camera_zoom.vz) / animation_frames;
}
