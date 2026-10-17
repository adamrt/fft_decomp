#include "fft/suzuki.h"
#include "psx/types.h"

/* SMD opcode 0xD7 "Vibrato Fade In": modulator 0 fade_level and its reset
 * value become 0x100 / (byte + 1). The duration wraps at 256;
 * byte 0xff leaves both values unchanged. */
u8* suzuki_smd_pitch_shift_fade_in(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u8 duration = note_data[0] + 1;
    s16 step;

    note_data++;
    if (duration != 0) {
        step = 0x100 / duration;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].fade_step = step;
        channel->modulators[SUZUKI_MODULATOR_SLOT_VIBRATO].fade_level = step;
    }
    return note_data;
}
