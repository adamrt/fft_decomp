#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_apply_undead_reversal(void) {
    battle_action_data_t* action;
    battle_action_data_t* action_again;
    u16 hp_damage;
    if (g_battle_action_target->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)) {
        g_battle_action_target_data->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
        g_current_ability_canceled_statuses[0] = 0;
        return;
    }
    action = g_battle_action_target_data;
    action->attack_type = action->attack_type & ~BATTLE_ACTION_TYPE_HP_DAMAGE;
    hp_damage = action->hp_damage;
    action_again = g_battle_action_target_data;
    action->hp_damage = 0;
    action->hp_healing = hp_damage;
    action_again->attack_type = action_again->attack_type | BATTLE_ACTION_TYPE_HP_HEALING;
}
