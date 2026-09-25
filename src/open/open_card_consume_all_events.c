#include "fft/open.h"

void open_card_consume_all_events(void) {
    open_card_consume_bios_events();
    open_card_consume_hardware_events();
}
