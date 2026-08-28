#include "fft/battle.h"

void battle_state_start_map_jump_out(s32 map_id, s32 duration) {
    s32 previous_state;

    previous_state = g_battle_game_state;
    g_battle_map_id = map_id;
    g_battle_game_state = BATTLE_GAME_STATE_MAP_JUMPING_OUT;
    g_previous_battle_game_state = previous_state;
    g_battle_state_map_transition_step = duration != 0 ? 0x100 / duration : 0x100;
}
