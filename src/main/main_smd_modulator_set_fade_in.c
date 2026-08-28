#include "fft/main_sound.h"
#include "psx/types.h"

/* SMD opcode 0xF2: for the modulator selected by 0xF0,
 * the first byte is the start delay and counter_18 and its reset value
 * become 0x100 / (second byte + 1). Generic twin of 0xD7, 0xE3 and 0xEB. */
u8* main_smd_modulator_set_fade_in(u8* note_data, void* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    s16 divisor;

    divisor = (note_data[1] + 1) & 0xff;
    modulator = &channel->modulators[channel->modulator_index];
    if (divisor != 0) {
        divisor = 0x100 / divisor;
        modulator->counter_14_reset = note_data[0];
        modulator->counter_18_reset = divisor;
        modulator->counter_18 = divisor;
    }
    return note_data + 2;
}
