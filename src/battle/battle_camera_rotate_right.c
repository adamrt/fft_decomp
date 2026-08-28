#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_rotate_right(s32 target_angle) {
    g_battle_previous_camera_yaw = g_battle_camera_render_state.vy & 0xfe00;
    g_battle_destination_camera_yaw = target_angle;
    if ((target_angle & 0x3ff) == 0) {
        g_battle_destination_camera_yaw = target_angle - 0x200;
    }
}
