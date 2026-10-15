#include "fft/battle_action.h"
#include "fft/battle_formula.h"

/* Effect of each dance. XA + YA is the weapon's XA plus its power
 * (formula_calculate_base_xa); the stat dances take 1. */
void battle_formula_apply_dance_abilities(void) {
    switch (g_action_current_ability.ability_id) {
    case ABILITY_ID_DANCE_WITCH_HUNT:
        g_action_target_data->mp_damage = g_action_current_ability.xa + g_action_current_ability.ya;
        g_action_target_data->attack_type = ACTION_TYPE_MP_DAMAGE;
        return;
    case ABILITY_ID_DANCE_WIZNAIBUS:
        g_action_target_data->hp_damage = g_action_current_ability.xa + g_action_current_ability.ya;
        g_action_target_data->attack_type = ACTION_TYPE_HP_DAMAGE;
        return;
    case ABILITY_ID_DANCE_SLOW_DANCE:
        g_action_target_data->sp_change = 1;
        break;
    case ABILITY_ID_DANCE_POLKA_POLKA:
        g_action_target_data->pa_change = 1;
        break;
    case ABILITY_ID_DANCE_DISILLUSION:
        g_action_target_data->ma_change = 1;
        break;
    case ABILITY_ID_DANCE_NAMELESS_DANCE:
        battle_formula_apply_status_to_action();
        return;
    case ABILITY_ID_DANCE_LAST_DANCE:
        g_action_target_data->ct_change = ACTION_CT_CHANGE_ZERO;
        break;
    }
    g_action_target_data->attack_type = ACTION_TYPE_PSEUDO_STATUS;
}
