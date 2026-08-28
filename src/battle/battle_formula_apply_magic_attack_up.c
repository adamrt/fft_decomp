#include "fft/battle.h"

void battle_formula_apply_magic_attack_up(void) {
    if (g_battle_action_attacker->support_abilities[1] & BATTLE_SUPPORT_SET_2_MAGIC_ATTACK_UP) {
        s16* xa = (s16*)&g_current_ability.xa;

        *xa = *xa * 4 / 3;
    }
}
