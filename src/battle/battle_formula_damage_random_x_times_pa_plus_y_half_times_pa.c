#include "fft/battle.h"
#include "psx/types.h"

void battle_formula_damage_random_x_times_pa_plus_y_half_times_pa(void) {
    battle_action_data_t* action;
    u8* mult;
    s32 rnd;

    if (battle_formula_calculate_physical_evade() != 0)
        return;
    battle_formula_store_pa_and_pa_plus_y_divided_by_two();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_physical_status_support_compatibility();
    battle_formula_calculate_critical_hit();
    battle_formula_store_xa_plus_ya_status_damage();
    rnd = battle_formula_get_random_0_7fff();
    mult = &g_current_ability.random_damage_factor;
    *mult = (s32)(rnd * g_current_ability.range_data.x) / 32768 + 1;
    action = g_battle_action_target_data;
    /* The target loads hp_damage signed (lh) here; keep the cast and the
       multiplier deref separated so `* *` never reads as a comment. */
    *(s16*)&action->hp_damage = *(s16*)&action->hp_damage * (*mult);
    action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
}
