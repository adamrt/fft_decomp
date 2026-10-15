#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_status.h"
#include "fft/battle_target.h"
#include "psx/types.h"

/* Scale the pending formula XA by the attacker/target zodiac compatibility.
 *
 * Serpentarius on either side leaves XA untouched.  The opposition entry
 * resolves by sex: monsters on either side fall back to the ordinary bad
 * result, otherwise same-sex pairs get the worst result and opposite-sex pairs
 * the best.
 *
 * Good and bad change XA by a quarter, best and worst by a half (XA * 25 / 100
 * or / 50, truncated); the result is at least 1.
 *
 * Casting the attacker sign to its stored 4-bit domain preserves the target's
 * distinct comparison copy without a compiler constraint.
 */
void formula_apply_zodiac_compatibility(void) {
    unit_stats_t* attacker;
    unit_stats_t* target;
    u16 attacker_zodiac;
    u32 attacker_zodiac_compare;
    u16 target_zodiac;
    u32 serpentarius = ZODIAC_SIGN_SERPENTARIUS;
    u16 difference;
    u8 compatibility;
    s16 xa;
    u16 product;

    attacker = g_action_attacker;
    target = g_action_target;
    attacker_zodiac = attacker->birthday.zodiac;
    target_zodiac = target->birthday.zodiac;
    attacker_zodiac_compare = (u8)attacker_zodiac;
    if (serpentarius == attacker_zodiac_compare) {
        return;
    }
    if (target_zodiac == serpentarius) {
        return;
    }

    if (target_zodiac < attacker_zodiac_compare) {
        difference = attacker_zodiac - target_zodiac;
    } else {
        difference = target_zodiac - attacker_zodiac;
    }

    compatibility = g_battle_zodiac_compatibility_modifiers[difference];
    if (compatibility == ZODIAC_COMPATIBILITY_GENDER_DEPENDENT) {
        u8 attacker_flags = g_action_attacker->unit_flags;

        compatibility = ZODIAC_COMPATIBILITY_BAD;
        if (!(attacker_flags & UNIT_FLAG_MONSTER)) {
            u8 target_flags = g_action_target->unit_flags;

            if (!(target_flags & UNIT_FLAG_MONSTER)) {
                compatibility = ZODIAC_COMPATIBILITY_BEST;
                if ((attacker_flags & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE))
                    == (target_flags & (UNIT_FLAG_MALE | UNIT_FLAG_FEMALE))) {
                    compatibility = ZODIAC_COMPATIBILITY_WORST;
                }
            }
        }
    }

    xa = g_action_current_ability.xa;
    product = xa * 25;
    switch (compatibility) {
    case ZODIAC_COMPATIBILITY_GOOD:
        g_action_current_ability.xa = xa + product / 100;
        break;
    case ZODIAC_COMPATIBILITY_BEST:
        g_action_current_ability.xa = xa + product / 50;
        break;
    case ZODIAC_COMPATIBILITY_BAD:
        g_action_current_ability.xa = xa - product / 100;
        break;
    case ZODIAC_COMPATIBILITY_WORST:
        g_action_current_ability.xa = xa - product / 50;
        break;
    }

    if (g_action_current_ability.xa <= 0) {
        g_action_current_ability.xa = 1;
    }
}
