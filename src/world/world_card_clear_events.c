#include "fft/world.h"

void world_card_clear_events(void) {
    world_card_consume_bios_events();
    world_card_consume_hardware_events();
}
