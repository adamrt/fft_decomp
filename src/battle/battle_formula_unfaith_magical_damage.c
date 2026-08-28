#include "fft/battle.h"
#include "psx/types.h"

/* volatile: the target reloads each unit pointer before every status test. */
extern battle_stats_t* volatile g_battle_action_attacker;
extern battle_stats_t* volatile g_battle_action_target;

/* Formula 1F: damage (MA + Y) * MA / 2 scaled by (100 - caster faith) and
 * (100 - target faith), with Faith treated as 0 and Innocent as 100. */
void battle_formula_unfaith_magical_damage(void) {
    battle_action_data_t* action;

    if (battle_formula_calculate_magical_evade() == 0) {
        battle_formula_store_ma_and_ma_plus_y_divided_by_two();
        battle_formula_apply_elemental_strengthen();
        battle_formula_apply_magical_support_status_compatibility();
        if (battle_formula_calculate_elemental_xa_times_ya() == 0) {
            if (g_battle_action_attacker->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FAITH)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FAITH)) {
                g_current_ability.attacker_faith = 0;
            }
            if (g_battle_action_attacker->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_INNOCENT)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INNOCENT)) {
                g_current_ability.attacker_faith = 100;
            }
            if (g_battle_action_target->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_FAITH)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_FAITH)) {
                g_current_ability.target_faith = 0;
            }
            if (g_battle_action_target->status_sets.current[BATTLE_STATUS_BYTE_INDEX(BATTLE_STATUS_ID_INNOCENT)]
                & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INNOCENT)) {
                g_current_ability.target_faith = 100;
            }
            action = g_battle_action_target_data;
            action->hp_damage = (s16)action->hp_damage * (100 - g_current_ability.target_faith)
                * (100 - g_current_ability.attacker_faith) / 10000;
            if (battle_formula_apply_elemental_absorption_and_status_proc() == 0) {
                battle_formula_apply_status();
            }
        }
    }
}
