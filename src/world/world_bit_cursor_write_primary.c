#include "fft/world.h"
#include "psx/types.h"

/* Writes one bit through the primary cursor, MSB first, advancing the cursor
 * after every 8 bits. */
void world_bit_cursor_write_primary(u8 bit) {
    s32 shift;
    s32 mask;
    u8* cursor;

    if (g_world_primary_bit_cursor_reset != 0) {
        g_world_primary_bit_write_index = 7;
        g_world_primary_bit_cursor_reset = 0;
    }
    shift = g_world_primary_bit_write_index;
    cursor = g_world_primary_bit_cursor;
    g_world_primary_bit_write_index = shift - 1;
    mask = 1 << shift;
    *cursor &= ~mask;
    if (bit) {
        *g_world_primary_bit_cursor = mask | *g_world_primary_bit_cursor;
    }
    if (g_world_primary_bit_write_index < 0) {
        g_world_primary_bit_write_index = 7;
        g_world_primary_bit_cursor = g_world_primary_bit_cursor + 1;
    }
}
