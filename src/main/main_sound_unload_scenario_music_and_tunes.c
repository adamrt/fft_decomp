#include "fft/main.h"

void main_sound_unload_scenario_music_and_tunes(void) {
    main_sound_stop_forced_music();
    main_sound_unload_scenario_mus(1);
    main_sound_unload_scenario_mus(2);
    main_sound_set_tune_volume(0);
}
