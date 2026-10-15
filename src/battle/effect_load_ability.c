#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Returns 1 when the ability has no effect file (effect id 0xffff), 0 for
 * Vertical Jump8, all Charge abilities and Accumulate. The target's range
 * starts at Vertical Jump8 rather than Charge +1. */
u32 effect_load_ability(s32 ability_id) {
    u32 result;

    if (ability_id >= ABILITY_ID_JUMP_VERTICAL_8 && ability_id < ABILITY_ID_MATH_FIRST) {
        return 0;
    }
    if (ability_id == ABILITY_ID_BASIC_SKILL_ACCUMULATE) {
        result = 0;
    } else {
        result = g_effect_ability_files[ability_id] < 0;
    }
    return result;
}
