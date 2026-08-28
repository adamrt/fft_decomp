#include "fft/battle.h"
#include "psx/types.h"

/* Convert the pending MP-cost field into an actual MP recovery event on
   the current action and mirror the amount onto the linked reaction
   record so the animation reports "MP restored". */
void battle_formula_apply_mp_recovery(void) {
    u16 mp_amount;
    battle_action_data_t* action;
    battle_action_data_t* action_after;

    action = g_battle_action_target_data;
    mp_amount = action->hp_damage;
    action->hp_damage = 0;
    action->attack_type = BATTLE_ACTION_TYPE_MP_DAMAGE;
    action->mp_damage = mp_amount;
    battle_action_finalize_target_current_action();
    action_after = g_battle_action_target_data;
    if (action_after->hit != 0) {
        battle_action_data_t* reaction = g_battle_action_attacker_data;
        reaction->mp_healing = action_after->mp_damage;
        reaction->attack_type = BATTLE_ACTION_TYPE_MP_HEALING;
        g_battle_action_attacker_data->hit = 1;
    }
}
