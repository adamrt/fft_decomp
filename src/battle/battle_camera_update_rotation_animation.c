#include "fft/battle.h"

void battle_camera_update_rotation_animation(void) {
    if (g_battle_camera_rotation_countdown != 0) {
        g_battle_camera_rotation_countdown--;
        g_battle_camera_render_state.vx += g_battle_camera_rotation_delta.vx;
        g_battle_camera_render_state.vy += g_battle_camera_rotation_delta.vy;
        g_battle_camera_render_state.vz += g_battle_camera_rotation_delta.vz;
    }
}
