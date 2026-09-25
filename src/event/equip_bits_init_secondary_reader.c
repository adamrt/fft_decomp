#include "fft/event_equip.h"
#include "psx/types.h"

void equip_bits_init_secondary_reader(const u8* data) {
    g_equip_bits_reader_2_reset = 1;
    g_equip_bits_reader_2_stream = data;
}
