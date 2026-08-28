#include "fft/battle.h"
#include "fft/data.h"
#include "psx/types.h"

void battle_action_check_damage_split_usability(void) {
    u16 hp;
    battle_action_data_t* action;

    hp = g_battle_action_target_data->hp_damage;
    if (hp != 0 && battle_action_calculate_chance_to_react(g_battle_action_target) == 0) {
        hp++;
        hp = ((u16)hp) >> 1;
        action = g_battle_action_target_data;
        action->last_received_attack = hp;
        action->reaction_id = ABILITY_ID_REACTION_DAMAGE_SPLIT;
    }
}
