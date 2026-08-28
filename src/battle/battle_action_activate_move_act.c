#include "fft/battle.h"

void battle_action_activate_move_act(battle_stats_t* unit) {
    unit->movement_taken = 0;
    unit->action_taken = 0;
}
