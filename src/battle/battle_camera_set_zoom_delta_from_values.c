#include "fft/battle.h"

void battle_camera_set_zoom_delta_from_values(const VECTOR* values, s32 frame_count) {
    s32 animation_frames;

    animation_frames = (frame_count & 0xffff) / g_animation_speed;
    g_battle_camera_zoom_countdown = animation_frames;
    animation_frames = g_battle_camera_zoom_countdown;
    g_battle_camera_zoom_delta.vx = values->vx / animation_frames;
    g_battle_camera_zoom_delta.vy = values->vy / animation_frames;
    g_battle_camera_zoom_delta.vz = values->vz / animation_frames;
}
