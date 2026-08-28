#include "fft/main_sound.h"

int main_sound_get_current_scenario_music(void) {
    return g_main_sound_music.state.scenario_track;
}
