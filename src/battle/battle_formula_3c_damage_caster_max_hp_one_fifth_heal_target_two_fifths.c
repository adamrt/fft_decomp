#include "fft/battle.h"
#include "psx/types.h"

/* Formula 3C: damage the caster for MaxHP/5 and heal the target for twice
 * that amount. */
void battle_formula_3c_damage_caster_max_hp_one_fifth_heal_target_two_fifths(void) {
    u16 amount;
    s32 dealt;
    battle_action_data_t* action;
    battle_action_data_t* target;

    amount = g_battle_action_attacker->max_hp / 5;
    action = g_battle_action_attacker_data;
    action->attack_type = BATTLE_ACTION_TYPE_HP_DAMAGE;
    action->hp_damage = amount;
    action = g_battle_action_attacker_data;
    action->hit = 1;
    target = g_battle_action_target_data;
    dealt = (s16)g_battle_action_attacker_data->hp_damage;
    target->attack_type = BATTLE_ACTION_TYPE_HP_HEALING;
    target->hp_healing = dealt * 2;
}
