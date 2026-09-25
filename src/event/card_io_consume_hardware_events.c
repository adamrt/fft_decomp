#include "fft/event_card.h"
#include "psx/types.h"

void card_io_consume_hardware_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD]);
}
