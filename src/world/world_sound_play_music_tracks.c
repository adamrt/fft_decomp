#include "fft/main_sound.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Unload the current scenario music, then load and start the requested music
 * and tune through the main-stack dispatcher, waiting for each load to
 * finish before switching to track 1. */
void world_sound_play_music_tracks(s32 scenario_music, s32 scenario_tune) {
    main_sound_unload_scenario_music_and_tunes();
    if (scenario_music != 0) {
        g_world_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        world_thread_call_on_main_stack(scenario_music, 1);
        world_sound_wait_for_music_stub();
    }
    if (scenario_tune != 0) {
        g_world_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        world_thread_call_on_main_stack(scenario_tune, 2);
        world_sound_wait_for_music_stub();
    }
    if (scenario_music != 0) {
        g_world_thread_call_target = (void (*)(void))main_sound_switch_music_track;
        world_thread_call_on_main_stack(1, 0x7f, 0);
    }
}
