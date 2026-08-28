#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Queue the CloseBattle game state; callers always pass transition step 8. */
void battle_state_halve_animation_speed_and_queue_close(s32 transition_step, s32 close_flow_state) {
    g_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_CLOSE_BATTLE;
    g_battle_state_map_transition_step = transition_step;
    g_main_system_game_flow_state = close_flow_state;
}
