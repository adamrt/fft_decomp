#include "fft/battle.h"

void battle_action_call_attack_preparation(battle_ai_command_action_t* action) {
    battle_action_prepare_attack(action, action, 0);
}
