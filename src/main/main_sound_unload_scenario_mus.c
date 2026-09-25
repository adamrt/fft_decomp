#include "fft/main.h"
#include "psx/types.h"

s32 main_sound_unload_scenario_mus(s32 index) {
    if (g_main_sound_music.slots.smd[index] != 0) {
        SuzukiUnloadMUS((suzuki_music_t*)g_main_sound_music.slots.handles[index]);
        g_main_sound_music.slots.handles[index] = 0;
        main_heap_free_smd(g_main_sound_music.slots.smd[index]);
        g_main_sound_music.slots.smd[index] = 0;
        return 1;
    }
    return 0;
}
