#include "fft/event_bunit.h"
#include "psx/types.h"

void bunit_bits_init_reader(const u8* data) {
    g_bunit_bits_reader_reset = 1;
    g_bunit_bits_cursor = data;
}
