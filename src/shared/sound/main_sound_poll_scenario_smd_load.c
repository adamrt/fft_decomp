#include "fft/main.h"

/* Finish the asynchronous scenario load and create the music record for
 * loading_slot (1 or 2). */
int main_sound_poll_scenario_smd_load(void) {
    if (g_main_sound_music.slots.loading_slot == 0) {
        return 0;
    }
    if (main_file_is_still_loading() != 0) {
        return 1;
    }
    g_main_sound_music.slots.music[g_main_sound_music.slots.loading_slot - 1]
        = suzuki_smd_create_music(g_main_sound_music.slots.smd[g_main_sound_music.slots.loading_slot - 1]);
    g_main_sound_music.slots.loading_slot = 0;
    return 0;
}
