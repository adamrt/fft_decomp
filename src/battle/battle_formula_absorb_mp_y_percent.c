#include "fft/battle.h"
#include "psx/types.h"

/* Formula 0x0F: absorb (Y)% of max MP, hit F(MA+X)%. */
void battle_formula_absorb_mp_y_percent(void) {
    if (battle_formula_calculate_magical_evade() != 0) {
        return;
    }
    if (battle_formula_calculate_magic_accuracy_no_elemental() != 0) {
        return;
    }
    battle_formula_calculate_mp_percent_damage();
    g_battle_action_target_data->hp_damage = g_battle_action_target_data->mp_damage;
    battle_formula_apply_mp_recovery();
}
