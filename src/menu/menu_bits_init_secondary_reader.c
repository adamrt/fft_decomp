#include "fft/bits.h"
#include "fft/menu.h"

void menu_bits_init_secondary_reader(const u8* data) {
    g_bits_reader_2_reset = 1;
    g_bits_reader_2_stream = data;
}
