#include "fft/battle.h"
#include "fft/main_runtime.h"

void battle_state_halve_animation_speed_for_jump_out(void) {
    g_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT;
}
