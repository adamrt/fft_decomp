#include "fft/battle_effect.h"
#include "psx/types.h"

/* Wrap the current bow-shot yaw into (-0x800, 0x800] then bucket it into
   one of three arc quadrants: 0 = behind, 1 = side, 2 = ahead. */
s32 battle_effect_get_bow_shot_angle(void) {
    s16 angle = g_battle_effect_arctan_angle_mod;
    if (angle >= 0x801) {
        g_battle_effect_arctan_angle_mod = angle - ONE;
    }
    {
        s16 angle_after_hi = g_battle_effect_arctan_angle_mod;
        if (angle_after_hi < -0x800) {
            g_battle_effect_arctan_angle_mod = angle_after_hi + ONE;
        }
    }
    {
        s16 wrapped = g_battle_effect_arctan_angle_mod;
        if (wrapped < 0x2AB) {
            return wrapped >= -0x2AA;
        }
        return 2;
    }
}
