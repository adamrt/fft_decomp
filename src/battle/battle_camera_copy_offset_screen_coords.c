#include "fft/battle.h"

void battle_camera_copy_offset_screen_coords(void) {
    g_battle_camera_offset_screen_coords.vx = g_battle_offset_screen_coords.vx;
    g_battle_camera_offset_screen_coords.vy = g_battle_offset_screen_coords.vy;
    g_battle_camera_offset_screen_coords.vz = g_battle_offset_screen_coords.vz;
}
