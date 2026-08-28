#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_store_pa_and_x(void) {
    s32 pa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_PHYSICAL_ATTACK];
    s32 x = g_current_ability.range_data.x;

    g_current_ability.ya = x;
    g_current_ability.xa = pa;
}
