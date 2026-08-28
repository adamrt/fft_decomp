#include "fft/battle.h"
#include "psx/types.h"

void battle_action_clear_data(void) {
    battle_action_clear_current_data(g_battle_action_target_data);
    battle_action_clear_current_data(g_battle_action_attacker_data);
    g_battle_action_attacker_data->hit = 0;
}
