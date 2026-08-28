#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x51: hit (MA+X)%. */
void battle_formula_51_hit_ma_x_percent(void) {
    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    battle_formula_apply_magic_attack_up();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit != 0) {
        battle_formula_apply_status_to_action();
    }
}
