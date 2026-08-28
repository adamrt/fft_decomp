#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_action_check_mp_switch_usability(void) {
    battle_stats_t* unit;
    battle_action_data_t* action;
    battle_action_data_t* action2;

    /* The target reads hp_damage / mp_damage signed (lh). */
    if (*(s16*)&g_battle_action_target_data->hp_damage == 0) {
        return;
    }
    unit = g_battle_action_target;
    if (unit->mp == 0) {
        return;
    }
    if (battle_action_calculate_chance_to_react(g_battle_action_target) != 0) {
        return;
    }
    if (g_battle_action_state != BATTLE_ACTION_STATE_EXECUTE) {
        return;
    }
    action = g_battle_action_target_data;
    action->mp_damage = action->mp_damage + action->hp_damage;
    if (*(s16*)&action->mp_damage >= 0x3E8) {
        action->mp_damage = 0x3E7;
    }
    action2 = g_battle_action_target_data;
    action2->attack_type &= ~BATTLE_ACTION_TYPE_HP_DAMAGE;
    action = g_battle_action_target_data;
    action2->hp_damage = 0;
    action->attack_type |= BATTLE_ACTION_TYPE_MP_DAMAGE;
    g_battle_action_target_data->reaction_id = ABILITY_ID_REACTION_MP_SWITCH;
}
