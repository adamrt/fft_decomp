#include "fft/event_card.h"
#include "psx/types.h"

void card_bits_init_secondary_reader(const u8* data) {
    g_card_bits_reader_2_reset = 1;
    g_card_bits_reader_2_stream = data;
}
