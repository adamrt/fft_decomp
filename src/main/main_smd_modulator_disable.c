#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0xF7: deactivates the modulator
 * selected by the parameter byte. */
u8* main_smd_modulator_disable(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u32 index;

    index = *note_data;
    channel->modulators[index].flags &= ~1;
    return note_data + 1;
}
