#include "fft/main.h"

/* Replaces the playing scenario music: stops and unloads it, loads music
 * file music_id (main_music_e) into a free slot and starts it at full
 * volume. */
void main_sound_set_current_music_track(int music_id) {
    int slot;

    if (g_main_sound_music.state.current_smd != 0) {
        slot = g_main_sound_music.state.current_slot;
        main_sound_stop_current_music();
        main_sound_unload_scenario_mus(slot);
    }
    slot = main_sound_open_music_into_free_slot(music_id);
    if (slot != 0) {
        main_sound_switch_music_track(slot, 0x7f, 0);
    }
}
