#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x26: steal equipped item, hit (SP+X)%. */
void battle_formula_steal_equipment(void) {
    battle_action_data_t* action;

    if (battle_formula_apply_steal_break_might_sword_hardcoding() != 0) {
        battle_formula_nullify_action();
        g_battle_action_target_data->miss_type = BATTLE_ACTION_MISS_TYPE_FORCED_FAILURE;
        return;
    }
    if (battle_formula_calculate_physical_evade() != 0) {
        return;
    }
    battle_formula_store_speed_and_x();
    if (battle_formula_calculate_physical_status_accuracy() != 0) {
        return;
    }
    battle_formula_apply_maintenance();
    action = g_battle_action_target_data;
    if (action->hit != 0) {
        /* The halfword store clears all other special-effect flags. */
        action->special_effect = BATTLE_ACTION_SPECIAL_EFFECT_STEAL_ITEM;
    }
}
