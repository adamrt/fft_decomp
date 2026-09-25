#include "fft/battle.h"

void battle_state_halve_animation_speed_for_jump_in(void) {
    g_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_IN;
}
