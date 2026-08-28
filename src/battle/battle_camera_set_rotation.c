#include "fft/battle_camera.h"

void battle_camera_set_rotation(const battle_camera_rotation_t* rotation) {
    g_battle_camera_render_state.vx = rotation->x;
    g_battle_camera_render_state.vy = rotation->y;
    g_battle_camera_render_state.vz = rotation->z;
}
