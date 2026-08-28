#include "fft/battle.h"

/* volatile view: the target reloads this global at every use. */
extern battle_stats_t* volatile g_battle_action_target;

void battle_formula_apply_target_xa_affecting_statuses(void) {
    battle_stats_t* target;

    if (g_battle_action_target->status_sets.current[3] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_PROTECT)) {
        s16* xa = (s16*)&g_current_ability.xa;

        *xa = *xa * 2 / 3;
    }

    target = g_battle_action_target;
    if ((target->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_SLEEP))
        || (target->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHARGING))) {
        s16* xa = (s16*)&g_current_ability.xa;

        *xa = *xa * 3 / 2;
    }

    if (g_battle_action_target->status_sets.current[2]
        & (BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_CHICKEN) | BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FROG))) {
        s16* xa = (s16*)&g_current_ability.xa;

        *xa = *xa * 3 / 2;
    }
}
