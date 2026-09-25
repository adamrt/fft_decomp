#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_normalize_yaw_angle(void) {
    g_battle_script_variables[BATTLE_CAMERA_YAW_WORD_INDEX]
        = (g_battle_script_variables[BATTLE_CAMERA_YAW_WORD_INDEX] + 0xa000) & 0x0fff;
}
