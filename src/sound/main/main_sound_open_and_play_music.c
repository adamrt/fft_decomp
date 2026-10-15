#include "fft/main.h"
#include "psx/types.h"

s32 main_sound_open_and_play_music(s32 music_id, s32 slot) {
    suzuki_smd_header_t* smd;

    if (g_main_sound_music.slots.smd[slot - 1] == 0) {
        smd = main_file_get_smd(
            g_main_sound_scenario_smd_files[music_id].sector, g_main_sound_scenario_smd_files[music_id].size);
        g_main_sound_music.slots.smd[slot - 1] = smd;
        g_main_sound_music.slots.music[slot - 1] = suzuki_smd_create_music(smd);
        return slot;
    }
    return 0;
}
