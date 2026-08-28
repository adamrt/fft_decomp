#include "fft/battle.h"
#include "psx/types.h"

void battle_state_stop_game_flow(void) {
    g_battle_state_game_flow_running = 0;
}
