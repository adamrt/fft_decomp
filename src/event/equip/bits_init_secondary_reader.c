/* Starting the secondary bit reader on a new data stream. */

#include "fft/bits.h"
#include "psx/types.h"

/* Start the secondary bit reader on a new data stream. */
void bits_init_secondary_reader(const u8* data) {
    g_bits_reader_2_reset = 1;
    g_bits_reader_2_stream = data;
}
