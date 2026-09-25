#include "fft/main.h"

void main_sound_set_current_music_track(int track) {
    int slot;

    if (g_main_sound_music.state.forced_music != 0) {
        slot = g_main_sound_music.state.scenario_track;
        main_sound_stop_forced_music();
        main_sound_unload_scenario_mus(slot);
    }
    slot = main_sound_open_music_into_free_slot(track);
    if (slot != 0) {
        main_sound_switch_music_track(slot, 0x7f, 0);
    }
}
