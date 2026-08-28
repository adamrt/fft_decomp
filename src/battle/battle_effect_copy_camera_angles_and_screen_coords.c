#include "fft/battle.h"

/* Seed the four effect camera snapshots from the live camera.
 *
 * Each snapshot receives the rotation masked to 12 bits, the current real
 * coordinates in whole units (Q12 shifted down), and the camera zoom vector. */
void battle_effect_copy_camera_angles_and_screen_coords(void) {
    u16* rotation;
    VECTOR* coords;

    rotation = battle_camera_get_rotation();
    g_battle_effect_camera_rotation_saved.vx = g_battle_effect_camera_rotation_start.vx
        = g_battle_effect_camera_rotation_target.vx = g_battle_effect_camera_rotation_current.vx = rotation[0] & 0xfff;
    g_battle_effect_camera_rotation_saved.vy = g_battle_effect_camera_rotation_start.vy
        = g_battle_effect_camera_rotation_target.vy = g_battle_effect_camera_rotation_current.vy = rotation[1] & 0xfff;
    g_battle_effect_camera_rotation_saved.vz = g_battle_effect_camera_rotation_start.vz
        = g_battle_effect_camera_rotation_target.vz = g_battle_effect_camera_rotation_current.vz = rotation[2] & 0xfff;
    coords = battle_camera_get_current_real_coords();
    g_battle_effect_camera_position_saved.vx = g_battle_effect_camera_position_start.vx
        = g_battle_effect_camera_position_target.vx = g_battle_effect_camera_position_current.vx = coords->vx >> 12;
    g_battle_effect_camera_position_saved.vy = g_battle_effect_camera_position_start.vy
        = g_battle_effect_camera_position_target.vy = g_battle_effect_camera_position_current.vy = coords->vy >> 12;
    g_battle_effect_camera_position_saved.vz = g_battle_effect_camera_position_start.vz
        = g_battle_effect_camera_position_target.vz = g_battle_effect_camera_position_current.vz = coords->vz >> 12;
    g_battle_effect_camera_zoom_saved = g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target
        = g_battle_effect_camera_zoom_current = *battle_camera_get_zoom();
}
