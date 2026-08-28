#include "fft/world.h"
#include "psx/types.h"

/* Selects the primary bitstream and resets its next operation to bit 7. */
void world_bit_cursor_set_primary(u8* ptr) {
    g_world_primary_bit_cursor_reset = 1;
    g_world_primary_bit_cursor = ptr;
}
