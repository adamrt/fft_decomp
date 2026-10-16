#include "fft/bits.h"
#include "psx/types.h"

void bits_init_primary_reader_variant(const u8* data) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = data;
}
