#include "fft/battle_action.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Record the highest EXP award produced by the current action.
 *
 * Success starts at 10; kills divide 20 by KO count. Level difference and
 * Gained EXP Up then adjust the result, which is clamped to 1 through 99. */
void battle_unit_update_attacker_earned_experience(s32 outcome) {
    s16 base;
    s32 signed_exp;
    s32 divisor;
    unit_stats_t* attacker;
    s16 exp;
    s32 sum;

    if ((g_action_context == ACTION_CONTEXT_PRIMARY) && (outcome != 0)) {
        if (outcome == 1) {
            base = 10;
        } else {
            divisor = g_action_target->ko_count;
            base = 20;
            if (divisor != 0) {
                base = 20 / divisor;
            }
        }
        attacker = g_action_attacker;
        sum = (base + g_action_target->level) - attacker->level;
        exp = sum;
        if (exp <= 0) {
            exp = 1;
        }
        if (attacker->ability_flags.support[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP) {
            exp = exp * 2;
        }
        if (exp >= 100) {
            exp = 99;
        }
        signed_exp = exp << 16;
        if (g_action_current_ability.earned_experience < (signed_exp >> 16)) {
            g_action_current_ability.earned_experience = exp;
        }
    }
}
