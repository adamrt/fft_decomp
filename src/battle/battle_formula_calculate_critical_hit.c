#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_calculate_critical_hit(void) {
    s32 rnd;
    s16* xa;
    s16 base_xa;
    if (battle_formula_roll_random_at_least(0x64, 4) == 0) {
        g_battle_action_target_data->critical = 1;
        rnd = battle_formula_get_random_0_7fff();
        xa = (s16*)&g_current_ability.xa;
        base_xa = *xa;
        *xa = base_xa + (rnd * base_xa) / 32768;
        /* The target leaves `xa` in $a0 for the argument-less callee. */
        ((void (*)(s16*))battle_formula_calculate_knockback)(xa);
    }
}
