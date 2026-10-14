#include "fft/battle_action.h"
#include "fft/battle_formula.h"
#include "fft/unit.h"

/* Formula 0x0E: damage (Y)% of max HP, hit F(MA+X)%; magic evade. The
 * ability's status must apply. An undead target skips the evade and its magic
 * defenses and is healed instead; a target immune to Dead is missed.
 *
 * 0x801938e6: set when the current target is undead, read by this formula to
 * skip the magic-defense stage. */
void formula_damage_hp_percent_hit_faith_ma_x_percent(void) {
    if (g_action_target->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_UNDEAD)]
        & STATUS_BYTE_MASK(STATUS_ID_UNDEAD)) {
        g_action_current_ability.target_is_undead = 1;
    } else {
        g_action_current_ability.target_is_undead = 0;
        if (formula_calculate_magical_evade() != 0) {
            return;
        }
    }
    formula_store_ma_and_x();
    formula_apply_elemental_strengthen();
    formula_apply_magic_attack_up();
    if (g_action_current_ability.target_is_undead == 0) {
        formula_apply_magic_defense_up();
        formula_apply_target_magical_status_xa_modifiers();
    }
    formula_apply_zodiac_compatibility();
    formula_store_xa_plus_ya_status_damage();
    formula_calculate_faith();
    formula_use_hp_damage_as_action_hit_percent();
    if (g_action_target_data->hit == 0) {
        return;
    }
    if (formula_apply_status_and_check_undead() == 0) {
        return;
    }
    formula_calculate_hp_percent_damage();
    formula_apply_undead_absorb_attack();
}
