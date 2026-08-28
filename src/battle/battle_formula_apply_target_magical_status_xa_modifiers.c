#include "fft/battle.h"
#include "psx/types.h"

/* Scale magical XA by the target's Shell, Frog, and Chicken statuses. */
void battle_formula_apply_target_magical_status_xa_modifiers(void) {
    if (g_battle_action_target->status_sets.current[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SHELL)) {
        s16* xa = (s16*)&g_current_ability.xa;
        *xa = *xa * 2 / 3;
    }
    if (g_battle_action_target->status_sets.current[2]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))) {
        s16* xa = (s16*)&g_current_ability.xa;
        *xa = *xa * 3 / 2;
    }
}
