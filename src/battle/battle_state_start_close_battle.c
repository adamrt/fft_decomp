#include "fft/battle.h"

void battle_state_start_close_battle(s32 duration) {
    g_animation_speed = 2;
    g_battle_game_state = BATTLE_GAME_STATE_CLOSE_BATTLE;
    g_battle_state_map_transition_step = duration != 0 ? 0x100 / duration : 0x100;
    g_main_system_game_flow_state = 1;
}
