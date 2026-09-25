#include "fft/main.h"
#include "psx/types.h"

s32 main_sound_open_and_play_music(s32 file_index, s32 slot) {
    suzuki_smd_header_t* smd;

    if (g_main_sound_music.slots.smd[slot] == 0) {
        smd = main_file_get_smd(
            g_main_sound_scenario_smd_files[file_index].sector, g_main_sound_scenario_smd_files[file_index].size);
        g_main_sound_music.slots.smd[slot] = smd;
        g_main_sound_music.slots.handles[slot] = (s32)SuzukiPutPlaySMD(smd);
        return slot;
    }
    return 0;
}
