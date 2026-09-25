#include "fft/main.h"
#include "psx/types.h"

/* SMD opcode 0x97 "Time Signature": numerator then denominator.
 * The numerator is held at halfword width: `lbu` already zero-extends it,
 * so a u8 temporary would be masked again before the 16-bit stores. */
u8* main_smd_time_signature(u8* note_data, suzuki_music_t* music, suzuki_music_channel_t* channel) {
    u16 numerator;
    u8 denominator;

    numerator = note_data[0];
    denominator = note_data[1];
    music->ticks_per_beat = 0xc0 / (denominator * music->tick_divisor);
    music->beat_unit = denominator;
    music->beats_per_bar = numerator;
    music->beats_remaining = numerator;
    music->beat_ticks_36 = music->ticks_per_beat;
    return note_data + 2;
}
