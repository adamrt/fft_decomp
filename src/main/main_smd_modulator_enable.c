#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xF6: restarts and activates the
 * modulator selected by the parameter byte. */
u8* main_smd_modulator_enable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;

    modulator = &channel->modulators[*note_data++];
    main_smd_modulator_reset(modulator);
    modulator->flags |= 1;
    return note_data;
}
