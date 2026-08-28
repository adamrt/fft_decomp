#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_magic_accuracy(void) {
    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    battle_formula_apply_magical_support_status_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_calculate_faith();
    battle_formula_use_hp_damage_as_action_hit_percent();
    return g_battle_action_target_data->hit == 0;
}
