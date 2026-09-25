#include "fft/event_attack.h"

void attack_sound_play_scenario_music(s32 primary_track, s32 alternate_track) {
    main_sound_unload_scenario_music_and_tunes();

    if (primary_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        battle_thread_call_on_main_stack(primary_track, 1);
        attack_sound_wait_music_idle();
    }

    if (alternate_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        battle_thread_call_on_main_stack(alternate_track, 2);
        attack_sound_wait_music_idle();
    }

    if (primary_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_switch_music_track;
        battle_thread_call_on_main_stack(1, 0x7f, 0);
    }
}
