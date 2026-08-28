#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_store_ma_and_x(void) {
    s32 ma = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    s32 x = g_current_ability.range_data.x;

    g_current_ability.ya = x;
    g_current_ability.xa = ma;
}
