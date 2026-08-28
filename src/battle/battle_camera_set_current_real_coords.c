#include "fft/battle.h"

void battle_camera_set_current_real_coords(const VECTOR* coords) {
    g_battle_camera_current_real_coords.vx = coords->vx;
    g_battle_camera_current_real_coords.vy = coords->vy;
    g_battle_camera_current_real_coords.vz = coords->vz;
}
