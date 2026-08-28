#include "fft/battle.h"
#include "psx/types.h"

void battle_action_check_pa_save_ma_save_speed_save_regenerator_auto_potion_gilgame_heart_usability(s16 reaction_id) {
    battle_action_data_t* action;

    if ((g_battle_action_target_data->attack_type & BATTLE_ACTION_TYPE_HP_DAMAGE)
        && battle_action_calculate_chance_to_react(g_battle_action_target) == 0) {
        action = g_battle_action_target_data;
        action->reaction_id = reaction_id;
        action->last_received_attack = action->hp_damage;
    }
}
