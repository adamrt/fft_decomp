#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Restores a MUS record from its saved snapshot: queues the voices playing
 * now for key-off, copies the snapshot back, keeps the current sequence_ticks as
 * snapshot_restore_tick, forces a full voice update and retriggers the held notes. */
void suzuki_smd_restore_snapshot(suzuki_music_t* music) {
    s32 tick;

    if (music->snapshot != 0 && (music->status & SUZUKI_MUSIC_STATUS_SNAPSHOT) != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        g_suzuki_sound_music_key_off_voices |= suzuki_smd_get_active_voice_mask(music);
        tick = music->sequence_ticks;
        suzuki_smd_copy_snapshot(music, music->snapshot);
        music->snapshot_restore_tick = tick;
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000 | SUZUKI_VOICE_ATTR_ALL);
        music->key_off_mask = 0;
        music->key_on_mask = 0;
        suzuki_smd_retrigger_held_voices(music);
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}
