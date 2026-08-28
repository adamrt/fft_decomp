#include "fft/main_sound.h"
#include "fft/thread.h"
#include "psx/types.h"

void require_sound_play_scenario_music(s32 first_track, s32 second_track) {
    main_sound_unload_scenario_music_and_tunes();
    if (first_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        battle_thread_call_on_main_stack(first_track, 1);
        require_sound_wait_music_idle();
    }
    if (second_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        battle_thread_call_on_main_stack(second_track, 2);
        require_sound_wait_music_idle();
    }
    if (first_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_switch_music_track;
        battle_thread_call_on_main_stack(1, 0x7f, 0);
    }
}
