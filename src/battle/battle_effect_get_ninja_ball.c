#include "fft/battle.h"
#include "psx/types.h"

s32 battle_effect_get_ninja_ball(s32 elements) {
    s32 i;

    if (elements != 0) {
        for (i = 0; i < 9; i++) {
            if ((elements & g_battle_effect_ninja_ball_masks[i]) != 0) {
                break;
            }
        }
        elements = i;
    }
    return elements;
}
