#include "fft/battle.h"

void battle_camera_restore_rotation(void) {
    g_battle_camera_render_state.vx = g_battle_camera_rotation_backup.vx;
    g_battle_camera_render_state.vy = g_battle_camera_rotation_backup.vy;
    g_battle_camera_render_state.vz = g_battle_camera_rotation_backup.vz;
}
