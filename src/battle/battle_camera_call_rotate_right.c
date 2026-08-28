#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_call_rotate_right(s32 target_angle) {
    g_battle_camera_rotation_action = 2;
    g_battle_camera_rotation_increment = 1;
    battle_camera_rotate_right(target_angle);
}
