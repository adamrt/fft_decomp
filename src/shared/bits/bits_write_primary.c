#include "fft/event_card.h"
#include "psx/types.h"

/* Writes one bit (set when the low byte of `value` is nonzero) through the
 * primary cursor, MSB first, advancing the cursor after every 8 bits. */
void bits_write_primary(s32 value) {
    s32 bit_index;
    u8 mask;

    if (g_bits_primary_reset != 0) {
        g_bits_writer_1_index = 7;
        g_bits_primary_reset = 0;
    }
    bit_index = g_bits_writer_1_index;
    g_bits_writer_1_index = bit_index - 1;
    mask = 1 << bit_index;
    *g_bits_primary_cursor &= ~mask;
    if ((value & 0xff) != 0) {
        *g_bits_primary_cursor |= mask;
    }
    if (g_bits_writer_1_index < 0) {
        g_bits_writer_1_index = 7;
        g_bits_primary_cursor++;
    }
}
