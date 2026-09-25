#include "fft/battle.h"
#include "psx/types.h"

/* Returns 1 when the ability has no effect file (effect id 0xffff), 0 for
 * Vertical Jump8, all Charge abilities and Accumulate. The target's range
 * starts at Vertical Jump8 rather than Charge +1. */
u32 battle_effect_load_ability(s32 ability_id) {
    u32 result;
    s32 effect_id;

    if ((u32)(ability_id - ABILITY_ID_JUMP_VERTICAL_8) < (ABILITY_ID_MATH_FIRST - ABILITY_ID_JUMP_VERTICAL_8)) {
        return 0;
    }
    if (ability_id == ABILITY_ID_BASIC_SKILL_ACCUMULATE) {
        result = 0;
    } else {
        effect_id = g_battle_effect_ability_ids[ability_id];
        result = (u32)effect_id >> 31;
    }
    return result;
}
