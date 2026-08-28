#include "fft/main_sound.h"
#include "psx/types.h"

/* Sets the SFX music record's fixed header (status 2, id 0x7fff, eight
 * channels, field_1a 0x7f), then the common header defaults. */
void main_smd_init_sfx_music_header(suzuki_music_t* music) {
    music->status = 2;
    music->id = 0x7fff;
    music->field_14 = 1;
    music->tick_divisor = 1;
    music->channel_count = 8;
    music->field_17 = 0;
    music->waveset_id = 0;
    music->field_1a = 0x7f;
    main_smd_init_music_header(music);
}
