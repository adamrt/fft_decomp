/* Waiting for music loads and playing scenario tracks. */

#include "fft/music.h"
#include "fft/thread.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Wait one vertical blank at a time while the music is loading, checking on the main stack. The
 * check (music_is_loading_stub) always reports done, so this waits a single VSync. */
void music_wait_for_load(void) {
    do {
        VSync(0);
        g_thread_call_target = music_is_loading_stub;
    } while (thread_call_on_main_stack() != 0);
}

/* Load requested music IDs into slots 1 and 2, wait one VSync after each
 * load, and play slot 1 when its ID is nonzero. */
void music_play_tracks(s32 slot_1_music_id, s32 slot_2_music_id) {
    music_unload_scenario_and_tunes();

    if (slot_1_music_id != 0) {
        g_thread_call_target = (void (*)(void))music_load_into_slot;
        thread_call_on_main_stack(slot_1_music_id, 1);
        music_wait_for_load();
    }

    if (slot_2_music_id != 0) {
        g_thread_call_target = (void (*)(void))music_load_into_slot;
        thread_call_on_main_stack(slot_2_music_id, 2);
        music_wait_for_load();
    }

    if (slot_1_music_id != 0) {
        g_thread_call_target = (void (*)(void))music_switch_track;
        thread_call_on_main_stack(1, 0x7f, 0);
    }
}
