#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x29: opposite sex only, hit (MA+X)%. */
void battle_formula_opposite_sex_hit_ma_x_percent(void) {
    battle_formula_store_ma_and_x();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        if ((g_battle_action_target->unit_flags & (UNIT_FLAG_MONSTER | UNIT_FLAG_FEMALE | UNIT_FLAG_MALE))
            == (g_battle_action_attacker->unit_flags & (UNIT_FLAG_MONSTER | UNIT_FLAG_FEMALE | UNIT_FLAG_MALE))) {
            battle_formula_force_attack_miss();
        } else {
            battle_formula_apply_status_to_action();
        }
    }
}
