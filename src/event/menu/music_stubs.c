/* The music wait stub and music track playback. */

#include "fft/main.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libetc.h"

void sound_wait_for_music_stub(void) {
    do {
        VSync(0);
        g_thread_call_target = main_sound_is_music_loading_stub;
    } while (thread_call_on_main_stack() != 0);
}

/* Load requested music IDs into slots 1 and 2, wait one VSync after each
 * load, and play slot 1 when its ID is nonzero. */
void sound_play_music_tracks(s32 slot_1_music_id, s32 slot_2_music_id) {
    main_sound_unload_scenario_music_and_tunes();

    if (slot_1_music_id != 0) {
        g_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        thread_call_on_main_stack(slot_1_music_id, 1);
        sound_wait_for_music_stub();
    }

    if (slot_2_music_id != 0) {
        g_thread_call_target = (void (*)(void))main_sound_open_and_play_music;
        thread_call_on_main_stack(slot_2_music_id, 2);
        sound_wait_for_music_stub();
    }

    if (slot_1_music_id != 0) {
        g_thread_call_target = (void (*)(void))main_sound_switch_music_track;
        thread_call_on_main_stack(1, 0x7f, 0);
    }
}
