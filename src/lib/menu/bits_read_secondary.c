/* Reading a value of a given bit width, most significant bit first, from the secondary bitstream. */

#include "fft/bits.h"
#include "psx/types.h"

/* Reads a `count`-bit value, most significant bit first, from the secondary
 * bitstream. The first read after a reset starts at bit 7 of the current
 * byte. */
s32 bits_read_secondary(s32 count) {
    s32 result;
    u8* cursor;
    s32 bit;
    u8 byte;

    if (count == 0) {
        return 0;
    }
    if (g_bits_reader_2_reset != 0) {
        g_bits_reader_2_index = 7;
        g_bits_reader_2_reset = 0;
    }
    result = 0;
    for (count = count - 1; count != -1; count--) {
        cursor = g_bits_reader_2_stream;
        byte = *cursor;
        bit = g_bits_reader_2_index;
        g_bits_reader_2_index = bit - 1;
        if ((byte >> bit) & 1) {
            result |= 1 << count;
        }
        if (g_bits_reader_2_index < 0) {
            g_bits_reader_2_index = 7;
            g_bits_reader_2_stream = cursor + 1;
        }
    }
    return result;
}
