#include "fft/main.h"
#include "psx/types.h"

/* Copies the SMD header settings into a MUS record, applies its reverb and
 * resets the playback header. */
void main_smd_transfer_music_data(suzuki_music_t* music) {
    suzuki_smd_header_t* smd;

    music->status |= 1;
    smd = music->smd;
    music->id = smd->id;
    music->field_14 = smd->field_12;
    music->tick_divisor = smd->tick_divisor;
    music->channel_count = smd->channel_count;
    music->field_17 = smd->field_15;
    music->waveset_id = smd->waveset_id;
    music->field_1a = smd->field_18;
    music->reverb_mode = smd->reverb_mode;
    music->reverb_depth = smd->reverb_depth << 8;
    music->reverb_delay = smd->reverb_delay;
    music->reverb_feedback = smd->reverb_feedback;
    main_sound_set_reverb_mode(music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
    main_smd_init_music_header(music);
}
