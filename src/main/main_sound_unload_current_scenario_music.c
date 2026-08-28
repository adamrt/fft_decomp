#include "fft/main_sound.h"

void main_sound_unload_current_scenario_music(void) {
    int slot = g_main_sound_music.state.scenario_track;

    main_sound_stop_forced_music();
    main_sound_unload_scenario_mus(slot);
}
