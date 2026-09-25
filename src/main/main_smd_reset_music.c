#include "fft/main.h"
#include "psx/api.h"
#include "psx/etc.h"
#include "psx/types.h"

/* Restarts a music handle from the top of its sequence: re-reads the SMD
 * header and channels with the root-counter event disabled, fades the master
 * volume in from 0 and marks the record playing again. */
void main_smd_reset_music(suzuki_music_t* music, s16 volume, s16 fade) {
    if (music != 0) {
        music->status &= 0x7fff;
        DisableEvent(g_main_root_counter_2_event);
        main_smd_transfer_music_data(music);
        main_smd_init_channels(music);
        music->master_volume.value = 0;
        SuzukiCalcMusVolChange(music, volume, fade);
        main_smd_force_channel_func(music, 0x7000);
        music->status |= 0x8000;
        EnableEvent(g_main_root_counter_2_event);
    }
}
