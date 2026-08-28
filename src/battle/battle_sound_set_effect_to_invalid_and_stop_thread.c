#include "fft/battle.h"
#include "psx/types.h"

void battle_sound_set_effect_to_invalid_and_stop_thread(void) {
    battle_sound_set_effect_to_invalid();
    battle_thread_exit_current();
}
