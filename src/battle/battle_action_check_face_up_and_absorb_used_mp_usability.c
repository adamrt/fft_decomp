#include "fft/battle.h"
#include "psx/types.h"
void battle_action_check_face_up_and_absorb_used_mp_usability(s16 reaction_id) {
    u8* used_mp = &g_current_ability.mp_cost;
    battle_action_data_t* action;
    if (*used_mp != 0) {
        if (battle_action_calculate_chance_to_react(g_battle_action_target) == 0) {
            action = g_battle_action_target_data;
            action->reaction_id = reaction_id;
            action->last_received_attack = (s16)*used_mp;
        }
    }
}
