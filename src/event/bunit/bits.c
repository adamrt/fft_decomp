/* BUNIT reads the packed job and ability bitfields most-significant bit first. */

#include "fft/bits.h"
#include "psx/types.h"

/* bit reader */
static const u8* s_unit_list_bits_cursor;
extern s32 g_unit_list_bits_index;
extern u8 g_unit_list_bits_reader_reset;

/* Start the bit reader on a new data stream. */
void bits_init_reader(const u8* data) {
    g_unit_list_bits_reader_reset = 1;
    s_unit_list_bits_cursor = data;
}

/* Reads a `bit_count`-bit value (taken modulo 32), most significant bit
 * first, from the bitstream at g_unit_list_bits_cursor. The first read after a
 * reset starts at bit 7 of the current byte. */
u32 bits_read(s32 bit_count) {
    u32 accumulator;

    if (bit_count == 0) {
        return 0;
    }

    bit_count &= 0x1f;
    if (g_unit_list_bits_reader_reset != 0) {
        g_unit_list_bits_index = 7;
        g_unit_list_bits_reader_reset = 0;
    }

    bit_count--;
    accumulator = 0;
    if (bit_count != -1) {
        do {
            if ((*s_unit_list_bits_cursor >> g_unit_list_bits_index--) & 1) {
                accumulator |= 1 << bit_count;
            }
            bit_count--;
            if (g_unit_list_bits_index < 0) {
                g_unit_list_bits_index = 7;
                s_unit_list_bits_cursor++;
            }
        } while (bit_count != -1);
    }
    return accumulator;
}
