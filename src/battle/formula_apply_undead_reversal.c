#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "psx/types.h"

/* Healing step: the stored HP damage becomes healing. An undead target takes
 * it as damage instead, and the ability no longer counts as canceling the
 * statuses of the first status byte (Dead among them). */
void formula_apply_undead_reversal(void) {
    action_data_t* action;
    action_data_t* action_again;
    u16 hp_damage;
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
        g_action_canceled_statuses[0] = 0;
        return;
    }
    action = g_action_target_data;
    action->attack_type = action->attack_type & ~ACTION_TYPE_HP_DAMAGE;
    hp_damage = action->hp_damage;
    action_again = g_action_target_data;
    action->hp_damage = 0;
    action->hp_healing = hp_damage;
    action_again->attack_type = action_again->attack_type | ACTION_TYPE_HP_HEALING;
}
