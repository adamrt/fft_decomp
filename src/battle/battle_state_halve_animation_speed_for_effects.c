#include "fft/battle.h"
#include "psx/types.h"

void battle_state_halve_animation_speed_for_effects(void) {
    g_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_EFFECT;
    battle_action_set_at_list_active();
}
