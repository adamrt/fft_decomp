#include "fft/battle.h"

void battle_state_enter_jp_xp_gain(void) {
    battle_unit_misc_data_t* unit;

    g_animation_speed = 1;
    g_battle_game_state = BATTLE_GAME_STATE_JP_EXP_GAIN;
    unit = battle_unit_get_source_misc_data();
    if (unit != 0) {
        unit->state_frame_counter = 0;
    }
}
