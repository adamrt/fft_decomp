#include "fft/main.h"

void main_sound_unload_current_scenario_music(void) {
    int slot = g_main_sound_music.state.current_slot;

    main_sound_stop_current_music();
    main_sound_unload_scenario_mus(slot);
}
