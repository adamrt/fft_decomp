#include "fft/main.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Restores a MUS record from its saved snapshot: queues the voices playing
 * now for key-off, copies the snapshot back, keeps the current tick_24 as
 * tick_2c, forces a full voice update and retriggers the held notes. */
void main_smd_restore_snapshot(suzuki_music_t* music) {
    s32 tick;

    if (music->snapshot != 0 && (music->status & 0x10) != 0) {
        DisableEvent(g_main_root_counter_2_event);
        g_main_sound_music_key_off_voices |= SuzukiGetActiveChannels(music);
        tick = music->tick_24;
        main_smd_copy_snapshot(music, music->snapshot);
        music->tick_2c = tick;
        main_smd_force_channel_func(music, 0x71ff);
        music->key_off_mask = 0;
        music->key_on_mask = 0;
        main_smd_retrigger_held_voices(music);
        EnableEvent(g_main_root_counter_2_event);
    }
}
