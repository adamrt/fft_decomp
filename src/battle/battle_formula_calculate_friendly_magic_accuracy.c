#include "fft/battle.h"
#include "psx/types.h"

s32 battle_formula_calculate_friendly_magic_accuracy(void) {
    battle_formula_apply_magic_attack_up();
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_calculate_faith();
    battle_formula_use_hp_damage_as_action_hit_percent();
    return g_battle_action_target_data->hit == 0;
}
