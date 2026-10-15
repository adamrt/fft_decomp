#include "fft/main.h"

/* main_sound_open_and_play_music into the first free scenario slot (1 or
 * 2); returns 0 when both are taken. */
int main_sound_open_music_into_free_slot(int music_id) {
    int i;
    suzuki_smd_header_t* smd;

    for (i = 0; i < 2; i++) {
        if (g_main_sound_music.slots.smd[i] == 0) {
            smd = main_file_get_smd(
                g_main_sound_scenario_smd_files[music_id].sector, g_main_sound_scenario_smd_files[music_id].size);
            g_main_sound_music.slots.smd[i] = smd;
            g_main_sound_music.slots.music[i] = suzuki_smd_create_music(smd);
            return i + 1;
        }
    }
    return 0;
}
