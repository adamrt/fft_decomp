#include "fft/battle.h"
#include "psx/types.h"

/* Record the highest EXP award produced by the current action.
 *
 * Success starts at 10; kills divide 20 by KO count. Level difference and
 * Gained EXP Up then adjust the result, which is clamped to 1 through 99. */
void battle_unit_update_attacker_earned_experience(s32 outcome) {
    s16 base;
    s32 signed_exp;
    s32 divisor;
    battle_stats_t* attacker;
    u8* best;
    s16 exp;
    s32 sum;

    if ((g_battle_action_context == BATTLE_ACTION_CONTEXT_PRIMARY) && (outcome != 0)) {
        if (outcome == 1) {
            base = 10;
        } else {
            divisor = g_battle_action_target->ko_count;
            base = 20;
            if (divisor != 0) {
                base = 20 / divisor;
            }
        }
        attacker = g_battle_action_attacker;
        sum = (base + g_battle_action_target->level) - attacker->level;
        exp = sum;
        if (exp <= 0) {
            exp = 1;
        }
        if (attacker->support_abilities[1] & BATTLE_SUPPORT_SET_2_GAINED_EXP_UP) {
            exp = exp * 2;
        }
        if (exp >= 100) {
            exp = 99;
        }
        signed_exp = exp << 16;
        best = &g_current_ability.earned_experience;
        if (*best < (signed_exp >> 16)) {
            *best = exp;
        }
    }
}
