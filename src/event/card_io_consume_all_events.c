#include "fft/event_card.h"

void card_io_consume_all_events(void) {
    card_io_consume_bios_events();
    card_io_consume_hardware_events();
}
