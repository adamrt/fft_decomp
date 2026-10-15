#include "fft/bits.h"
#include "fft/world.h"
#include "psx/types.h"

/* Selects the primary bitstream and resets its next operation to bit 7. */
void bits_init_primary_reader_variant(const u8* data) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = (u8*)data;
}
