#include "fft/world.h"
#include "psx/types.h"

void world_card_consume_bios_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD]);
}
