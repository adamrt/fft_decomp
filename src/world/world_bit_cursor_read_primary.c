#include "fft/world.h"
#include "psx/types.h"

/* Reads an MSB-first value from the primary bitstream. */
s32 world_bit_cursor_read_primary(s32 bit_count) {
    s32 result;
    s32 shift;
    s32 byte;
    u8* cursor;

    if (bit_count == 0) {
        return 0;
    }
    result = 0;
    if (g_world_primary_bit_cursor_reset != 0) {
        g_world_primary_bit_read_index = 7;
        g_world_primary_bit_cursor_reset = 0;
    }
    bit_count -= 1;
    while (bit_count != -1) {
        cursor = g_world_primary_bit_cursor;
        byte = *cursor;
        shift = g_world_primary_bit_read_index;
        g_world_primary_bit_read_index = shift - 1;
        if ((byte >> shift) & 1) {
            result |= 1 << bit_count;
        }
        bit_count -= 1;
        if (g_world_primary_bit_read_index < 0) {
            g_world_primary_bit_read_index = 7;
            g_world_primary_bit_cursor = cursor + 1;
        }
    }
    return result;
}
