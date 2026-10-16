#include "psx/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xF2 "Modulator Set Fade In": for the modulator selected by
 * 0xF0, the first byte is the start delay and fade_level and its reset value
 * become 0x100 / (second byte + 1). Generic twin of 0xD7, 0xE3 and 0xEB;
 * second byte 0xff leaves the delay and both fade values unchanged. */
u8* suzuki_smd_modulator_set_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    suzuki_modulator_t* modulator;
    u8 duration = note_data[1] + 1;
    s16 step;

    modulator = &channel->modulators[channel->modulator_index];
    if (duration != 0) {
        step = 0x100 / duration;
        modulator->delay_reset = note_data[0];
        modulator->fade_step = step;
        modulator->fade_level = step;
    }
    return note_data + 2;
}
