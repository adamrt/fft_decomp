#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_sound_change_scenario_music(s32 scenario) {
    u16 packed = g_wldcore_active_saved_record.music_selection;
    s32 slot = (s8)(packed >> 8);

    if ((packed & 0xFF) != 0) {
        main_sound_stop_forced_music();
    }
    if (slot != 0) {
        main_sound_unload_scenario_mus(slot);
    } else {
        slot = 2;
    }
    VSync(2);
    main_sound_open_and_play_music(scenario, slot);
    g_wldcore_active_saved_record.music_selection = (slot << 8) | scenario;
    while (main_return_zero_80043708() != 0) {
        VSync(0);
    }
    g_wldcore_audio_queue.music.tracks[slot] = scenario;
    g_wldcore_audio_queue.current_slot = slot;
}
