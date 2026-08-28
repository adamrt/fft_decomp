#include "fft/battle.h"

void battle_formula_apply_attacker_berserk_frog(void) {
    if (g_battle_action_attacker->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_BERSERK)) {
        s16* xa = (s16*)&g_current_ability.xa;

        *xa = *xa * 3 / 2;
    }
    if (g_battle_action_attacker->status_sets.current[2] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG)) {
        g_current_ability.xa = 1;
    }
}
