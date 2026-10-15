#include "fft/bits.h"
#include "fft/event_equip.h"

void equip_bits_init_primary_reader(const u8* data) {
    g_bits_primary_reset = 1;
    g_bits_primary_cursor = data;
}
