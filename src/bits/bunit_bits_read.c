#include "fft/event_bunit.h"
#include "psx/types.h"

/* Reads a `bit_count`-bit value (taken modulo 32), most significant bit
 * first, from the bitstream at g_bunit_bits_cursor. The first read after a
 * reset starts at bit 7 of the current byte. */
u32 bunit_bits_read(s32 bit_count) {
    u32 accumulator;

    if (bit_count == 0) {
        return 0;
    }

    bit_count &= 0x1f;
    if (g_bunit_bits_reader_reset != 0) {
        g_bunit_bits_index = 7;
        g_bunit_bits_reader_reset = 0;
    }

    bit_count--;
    accumulator = 0;
    if (bit_count != -1) {
        do {
            if ((*g_bunit_bits_cursor >> g_bunit_bits_index--) & 1) {
                accumulator |= 1 << bit_count;
            }
            bit_count--;
            if (g_bunit_bits_index < 0) {
                g_bunit_bits_index = 7;
                g_bunit_bits_cursor++;
            }
        } while (bit_count != -1);
    }
    return accumulator;
}
