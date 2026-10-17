#include "fft/suzuki.h"

/* Copies the SMD header settings into a MUS record, applies its reverb and
 * resets the playback header. */
void suzuki_smd_transfer_music_data(suzuki_music_t* music) {
    suzuki_smd_header_t* smd;

    music->status |= SUZUKI_MUSIC_STATUS_SMD;
    smd = music->smd;
    music->id = smd->id;
    music->_unknown_014 = smd->_unknown_12;
    music->tick_divisor = smd->tick_divisor;
    music->channel_count = smd->channel_count;
    music->_unknown_017 = smd->_unknown_15;
    music->waveset_id = smd->waveset_id;
    music->_unknown_01a = smd->_unknown_18;
    music->reverb_mode = smd->reverb_mode;
    music->reverb_depth = smd->reverb_depth << 8;
    music->reverb_delay = smd->reverb_delay;
    music->reverb_feedback = smd->reverb_feedback;
    suzuki_sound_set_reverb_mode(music->reverb_mode, music->reverb_depth, music->reverb_delay, music->reverb_feedback);
    suzuki_smd_init_music_header(music);
}
