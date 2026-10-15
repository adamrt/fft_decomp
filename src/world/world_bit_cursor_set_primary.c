#include "fft/bits.h"
#include "fft/world.h"
#include "psx/types.h"

/* Selects the primary bitstream and resets its next operation to bit 7. */
void world_bit_cursor_set_primary(u8* ptr) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = ptr;
}
