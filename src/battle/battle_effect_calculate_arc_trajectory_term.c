#include "fft/battle.h"
#include "psx/types.h"

/*
 * Evaluate the arcing-weapon trajectory term a * b - (1 + a^2) * b^2 /
 * (2 * g_battle_effect_gravity_modifier) in 20.12 fixed point.
 *
 * Both operands are pre-shifted by range so the squares stay within 32 bits.
 * g_battle_effect_gravity_modifier is the trajectory gravity modifier
 * (0xa8000). The operand meanings are not yet verified, so they keep neutral names.
 */
s32 battle_effect_calculate_arc_trajectory_term(s32 height, s32 progress) {
    s32 divisor;
    s32 height_sq;
    s32 progress_sq;
    s32 linear;

    divisor = (g_battle_effect_gravity_modifier * 2) >> 12;
    if ((u32)(height + 0x7fff) <= 0xfffe) {
        height_sq = (height * height) >> 12;
        if ((u32)(progress + 0xfffff) <= 0x1ffffe) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (height * (progress >> 5)) >> 7;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (height * (progress >> 7)) >> 5;
        }
    } else {
        height >>= 4;
        height_sq = (height * height) >> 4;
        if ((u32)(progress + 0xfffff) <= 0x1ffffe) {
            progress_sq = (progress >> 5) * (progress >> 5);
            progress_sq >>= 2;
            linear = (height * (progress >> 5)) >> 3;
        } else {
            progress_sq = (progress >> 7) * (progress >> 7);
            progress_sq <<= 2;
            linear = (height * (progress >> 7)) >> 1;
        }
    }
    return linear - (((height_sq + ONE) >> 6) * (progress_sq >> 6)) / divisor;
}
