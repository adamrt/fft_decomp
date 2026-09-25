#include "fft/battle.h"
#include "psx/types.h"

s32 battle_camera_wrap_yaw_angle(void) {
    s32 yaw;

    yaw = g_battle_script_variables[BATTLE_CAMERA_YAW_WORD_INDEX];
    while (yaw < 0) {
        yaw += ONE;
    }
    return yaw;
}
