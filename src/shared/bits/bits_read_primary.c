#include "fft/event_equip.h"

/* Reads a `count`-bit value, most significant bit first, from the primary
 * bitstream. The first read after a reset starts at bit 7 of the current
 * byte. */
u32 bits_read_primary(s32 count) {
    u32 result;
    u8* cursor;
    s32 bit;
    u8 byte;

    if (count == 0) {
        return 0;
    }
    if (g_bits_primary_reset != 0) {
        g_bits_reader_1_index = 7;
        g_bits_primary_reset = 0;
    }
    result = 0;
    for (count = count - 1; count != -1; count--) {
        cursor = g_bits_primary_cursor;
        byte = *cursor;
        bit = g_bits_reader_1_index;
        g_bits_reader_1_index = bit - 1;
        if ((byte >> bit) & 1) {
            result |= 1 << count;
        }
        if (g_bits_reader_1_index < 0) {
            g_bits_reader_1_index = 7;
            g_bits_primary_cursor = cursor + 1;
        }
    }
    return result;
}
