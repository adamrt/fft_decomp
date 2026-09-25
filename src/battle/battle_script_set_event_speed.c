#include "fft/battle.h"
#include "psx/types.h"

void battle_script_set_event_speed(s32 speed) {
    g_battle_event_speed = speed;
    battle_state_set_animation_speed(speed);
}
