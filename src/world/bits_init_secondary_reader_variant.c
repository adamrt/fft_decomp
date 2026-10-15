#include "fft/bits.h"
#include "fft/world.h"
#include "psx/types.h"

/* Selects the secondary read-only bitstream and resets it to bit 7. */
void bits_init_secondary_reader_variant(const u8* ptr) {
    g_bits_reader_2_reset = 1;
    g_bits_reader_2_stream = ptr;
}
