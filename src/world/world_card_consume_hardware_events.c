#include "fft/card.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

/* Twin of world_card_consume_bios_events (0x8013270c) over the hardware
 * event handles. */
void world_card_consume_hardware_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD]);
}
