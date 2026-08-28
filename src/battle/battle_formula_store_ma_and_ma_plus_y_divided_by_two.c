#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_store_ma_and_ma_plus_y_divided_by_two(void) {
    g_current_ability.xa = g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK];
    g_current_ability.ya
        = (g_battle_action_attacker->attributes[UNIT_ATTRIBUTE_MAGIC_ATTACK] + g_current_ability.range_data.y) >> 1;
}
