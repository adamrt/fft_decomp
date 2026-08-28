#include "fft/card.h"
#include "psx/types.h"

void card_bits_init_primary_reader(const u8* data) {
    g_card_bits_primary_reset = 1;
    g_card_bits_primary_cursor = data;
}
