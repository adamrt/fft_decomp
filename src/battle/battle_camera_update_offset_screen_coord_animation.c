#include "fft/battle.h"

void battle_camera_update_offset_screen_coord_animation(void) {
    s32 fixed_x;
    s32 fixed_z;
    s32 fixed_y;

    if (g_battle_camera_offset_screen_coord_countdown != 0) {
        fixed_x = g_battle_camera_offset_screen_coords_fixed.vx + g_battle_camera_real_coord_delta.vx;
        g_battle_camera_offset_screen_coords_fixed.vx = fixed_x;
        fixed_z = g_battle_camera_offset_screen_coords_fixed.vy + g_battle_camera_real_coord_delta.vy;
        fixed_y = g_battle_camera_offset_screen_coords_fixed.vz + g_battle_camera_real_coord_delta.vz;
        g_battle_camera_offset_screen_coords_fixed.vy = fixed_z;
        g_battle_camera_offset_screen_coords_fixed.vz = fixed_y;
        g_battle_offset_screen_coords.vx = fixed_x / ONE;
        g_battle_offset_screen_coords.vy = fixed_z / ONE;
        g_battle_offset_screen_coords.vz = fixed_y / ONE;
        g_battle_camera_offset_screen_coord_countdown--;
    }
}
