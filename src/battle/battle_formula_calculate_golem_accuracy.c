#include "fft/battle.h"
#include "psx/types.h"

/* volatile: the target reloads g_current_attacker before each status test
 * instead of keeping it in a register. */
extern battle_stats_t* volatile g_battle_action_attacker;

s32 battle_formula_calculate_golem_accuracy(void) {
    battle_action_data_t* action;

    battle_formula_store_ma_and_x();
    battle_formula_apply_magic_attack_up();
    battle_formula_store_xa_plus_ya_status_damage();
    if ((g_battle_action_attacker->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FAITH)) != 0) {
        g_current_ability.attacker_faith = 0x64;
    }
    if ((g_battle_action_attacker->status_sets.current[4] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INNOCENT)) != 0) {
        g_current_ability.attacker_faith = 0;
    }
    action = g_battle_action_target_data;
    /* The target reads hp_damage signed (lh). */
    action->hp_damage = (s16)(*(s16*)&action->hp_damage * g_current_ability.attacker_faith / 100);
    battle_formula_use_hp_damage_as_action_hit_percent();
    return g_battle_action_target_data->hit == 0;
}
