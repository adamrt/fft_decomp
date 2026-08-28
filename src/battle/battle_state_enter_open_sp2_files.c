#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_state_enter_open_sp2_files(void) {
    g_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_OPEN_SP2_FILES;
    g_battle_gfx_sp2_data = 0;
}
