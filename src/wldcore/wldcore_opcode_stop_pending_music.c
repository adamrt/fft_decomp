#include "fft/main_sound.h"
#include "fft/wldcore.h"
#include "psx/types.h"

/* Script opcode: stop the pending music track, clear its state slot, and mark
 * the opcode complete. */
void wldcore_opcode_stop_pending_music(void) {
    u16* music = &g_wldcore_active_saved_record.music_selection;
    wldcore_audio_music_slots_t* state;
    s32* tracks;
    s16 track;

    track = *music;
    if (track != 0) {
        state = &g_wldcore_audio_queue.music;
        state->flags &= ~1;
        main_sound_stop_forced_music();
        main_sound_unload_scenario_mus((s16)*music >> 8);
        /* Indexed one word above the table wldcore_sound_change_scenario_music fills. */
        tracks = &state->tracks[1];
        tracks[(s16)*music >> 8] = 0;
        g_wldcore_audio_queue.current_slot = 0;
        *music = 0;
    }
    g_wldcore_active_saved_record.state_flags |= 4;
}
