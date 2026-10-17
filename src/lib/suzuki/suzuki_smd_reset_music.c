#include "fft/suzuki.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Restarts a music handle from the top of its sequence: re-reads the SMD
 * header and channels with the root-counter event disabled, fades the master
 * volume in from 0 and marks the record playing again. */
void suzuki_smd_reset_music(suzuki_music_t* music, s16 volume, s16 fade) {
    if (music != 0) {
        music->status &= ~SUZUKI_MUSIC_STATUS_PLAYING;
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        suzuki_smd_transfer_music_data(music);
        suzuki_smd_init_channels(music);
        music->master_volume.value = 0;
        suzuki_smd_set_music_volume(music, volume, fade);
        suzuki_smd_set_voice_attr_flags_all_channels(music, 0x7000);
        music->status |= SUZUKI_MUSIC_STATUS_PLAYING;
        EnableEvent(g_suzuki_sound_root_counter_2_event);
    }
}
