#include "fft/battle.h"
#include "psx/types.h"

s32 battle_state_set_animation_speed(s32 speed) {
    if ((u32)(speed - 1) < 2) {
        g_animation_speed = speed;
        return 0;
    }
    return -1;
}
