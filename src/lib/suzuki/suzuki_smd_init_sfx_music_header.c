#include "fft/suzuki.h"

/* Sets the SFX music record's fixed header (SUZUKI_MUSIC_STATUS_SFX, id
 * 0x7fff, eight channels, _unknown_01a 0x7f), then the common header
 * defaults. */
void suzuki_smd_init_sfx_music_header(suzuki_music_t* music) {
    music->status = SUZUKI_MUSIC_STATUS_SFX;
    music->id = 0x7fff;
    music->_unknown_014 = 1;
    music->tick_divisor = 1;
    music->channel_count = SUZUKI_SFX_CHANNEL_COUNT;
    music->_unknown_017 = 0;
    music->waveset_id = 0;
    music->_unknown_01a = 0x7f;
    suzuki_smd_init_music_header(music);
}
