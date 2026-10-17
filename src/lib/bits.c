/* Two independent bit streams used by menu and save records. */

#include "fft/bits.h"
#include "psx/types.h"

extern s32 g_bits_primary_read_index;
extern s32 g_bits_primary_write_index;
extern s32 g_bits_secondary_read_index;

/* WORLD keeps both initializers with its grid-cursor helpers. */
#ifndef FFT_WORLD
/* Start the primary bit reader on a new data stream. */
void bits_init_primary_reader(u8* data) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = data;
}

/* Start the secondary bit reader on a new data stream. */
void bits_init_secondary_reader(const u8* data) {
    g_bits_secondary_reset = 1;
    g_bits_secondary_cursor = data;
}
#endif

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
        g_bits_primary_read_index = 7;
        g_bits_primary_reset = 0;
    }
    result = 0;
    for (count = count - 1; count != -1; count--) {
        cursor = g_bits_primary_cursor;
        byte = *cursor;
        bit = g_bits_primary_read_index;
        g_bits_primary_read_index = bit - 1;
        if ((byte >> bit) & 1) {
            result |= 1 << count;
        }
        if (g_bits_primary_read_index < 0) {
            g_bits_primary_read_index = 7;
            g_bits_primary_cursor = cursor + 1;
        }
    }
    return result;
}

/* CARD and WORLD also write through the primary cursor. */
#if defined(FFT_EVENT_CARD) || defined(FFT_WORLD)
/* Writes one bit (set when the low byte of `value` is nonzero) through the
 * primary cursor, MSB first, advancing the cursor after every 8 bits. */
void bits_write_primary(s32 value) {
    s32 bit_index;
    u8 mask;

    if (g_bits_primary_reset != 0) {
        g_bits_primary_write_index = 7;
        g_bits_primary_reset = 0;
    }
    bit_index = g_bits_primary_write_index;
    g_bits_primary_write_index = bit_index - 1;
    mask = 1 << bit_index;
    *g_bits_primary_cursor &= ~mask;
    if ((value & 0xff) != 0) {
        *g_bits_primary_cursor |= mask;
    }
    if (g_bits_primary_write_index < 0) {
        g_bits_primary_write_index = 7;
        g_bits_primary_cursor++;
    }
}
#endif

/* Reads a `count`-bit value, most significant bit first, from the secondary
 * bitstream. The first read after a reset starts at bit 7 of the current
 * byte. */
s32 bits_read_secondary(s32 count) {
    s32 result;
    const u8* cursor;
    s32 bit;
    u8 byte;

    if (count == 0) {
        return 0;
    }
    if (g_bits_secondary_reset != 0) {
        g_bits_secondary_read_index = 7;
        g_bits_secondary_reset = 0;
    }
    result = 0;
    for (count = count - 1; count != -1; count--) {
        cursor = g_bits_secondary_cursor;
        byte = *cursor;
        bit = g_bits_secondary_read_index;
        g_bits_secondary_read_index = bit - 1;
        if ((byte >> bit) & 1) {
            result |= 1 << count;
        }
        if (g_bits_secondary_read_index < 0) {
            g_bits_secondary_read_index = 7;
            g_bits_secondary_cursor = cursor + 1;
        }
    }
    return result;
}
