#include "fft/battle.h"

void battle_formula_apply_elemental_strengthen(void) {
    if (g_battle_action_attacker->elemental_affinity[ELEMENTAL_AFFINITY_STRENGTHEN]
        & g_current_ability.range_data.element) {
        g_current_ability.xa = (s16)g_current_ability.xa * 5 / 4;
    }
}
