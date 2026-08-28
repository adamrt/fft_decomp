#include "fft/equip.h"

void equip_bits_init_primary_reader(const u8* data) {
    g_equip_bits_reader_1_reset = 1;
    g_equip_bits_reader_1_stream = data;
}
