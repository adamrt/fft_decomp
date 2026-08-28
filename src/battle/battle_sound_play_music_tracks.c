#include "fft/battle.h"
#include "fft/main_sound.h"
#include "fft/thread.h"
#include "psx/types.h"

/* BATTLE's copy of attack_sound_play_scenario_music (ATTACK.OUT 0x801c3e60). */
void battle_sound_play_music_tracks(s32 primary_track, s32 alternate_track) {
    main_sound_unload_scenario_music_and_tunes();

    if (primary_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        battle_thread_call_on_main_stack(primary_track, 1);
        battle_sound_wait_for_music_stub();
    }

    if (alternate_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        battle_thread_call_on_main_stack(alternate_track, 2);
        battle_sound_wait_for_music_stub();
    }

    if (primary_track != 0) {
        g_battle_thread_call_target = (void (*)(void))main_sound_switch_music_track;
        battle_thread_call_on_main_stack(1, 0x7f, 0);
    }
}
