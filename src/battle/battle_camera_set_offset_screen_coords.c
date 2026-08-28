#include "fft/battle.h"

void battle_camera_set_offset_screen_coords(VECTOR* coords) {
    g_battle_offset_screen_coords.vx = coords->vx;
    g_battle_offset_screen_coords.vy = coords->vy;
    g_battle_offset_screen_coords.vz = coords->vz;
}
