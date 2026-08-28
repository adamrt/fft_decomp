#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x33: hit (PA+X)%. */
void battle_formula_hit_pa_x_percent(void) {
    battle_formula_store_pa_and_x();
    battle_formula_apply_attack_up_and_martial_arts();
    battle_formula_apply_attacker_berserk_frog();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}
