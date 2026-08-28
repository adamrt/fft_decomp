#include "fft/battle.h"
#include "psx/types.h"

/* 0x801938e6: set when the current target is undead, read by this formula to
 * skip the magic-defense stage. */

void battle_formula_damage_hp_percent_hit_faith_ma_x_percent(void) {
    if (g_battle_action_target->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_UNDEAD)) {
        g_current_ability.target_is_undead = 1;
    } else {
        g_current_ability.target_is_undead = 0;
        if (battle_formula_calculate_magical_evade() != 0) {
            return;
        }
    }
    battle_formula_store_ma_and_x();
    battle_formula_apply_elemental_strengthen();
    battle_formula_apply_magic_attack_up();
    if (g_current_ability.target_is_undead == 0) {
        battle_formula_apply_magic_defense_up();
        battle_formula_apply_target_magical_status_xa_modifiers();
    }
    battle_formula_apply_zodiac_compatibility();
    battle_formula_store_xa_plus_ya_status_damage();
    battle_formula_calculate_faith();
    battle_formula_use_hp_damage_as_action_hit_percent();
    if (g_battle_action_target_data->hit == 0) {
        return;
    }
    if (battle_formula_apply_status_and_check_undead() == 0) {
        return;
    }
    battle_formula_calculate_hp_percent_damage();
    battle_formula_apply_undead_absorb_attack();
}
