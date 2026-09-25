#include "fft/main.h"

int main_sound_open_music_into_free_slot(int scenario) {
    int checked_slots = 0;
    /* Byte-offset form kept: indexing g_main_sound_scenario_smd_files[scenario]
     * schedules the scale away from where the target computes it. */
    int file_offset = scenario * 8;
    suzuki_smd_header_t** loaded_smd_base = &g_main_sound_music.slots.smd[1];
    s32* handle = &g_main_sound_music.slots.handles[1];
    suzuki_smd_header_t** loaded_smd = loaded_smd_base;
    suzuki_smd_header_t* smd;

    do {
        if (*loaded_smd == 0) {
            smd = main_file_get_smd(*(int*)((char*)g_main_sound_scenario_smd_files + file_offset),
                *(int*)((char*)g_main_sound_scenario_smd_files + file_offset + 4));
            *loaded_smd = smd;
            *handle = (int)SuzukiPutPlaySMD(smd);
            /* A plain `return` here lets GCC's loop pass move this
             * straight-line exit block after the loop; the target keeps it
             * inline, which the in-loop goto reproduces. */
            goto smd_opened;
        }
        handle++;
        checked_slots++;
        loaded_smd++;
        continue;
    smd_opened:
        return checked_slots + 1;
    } while (checked_slots < 2);
    return 0;
}
