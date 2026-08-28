#include "fft/card.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

s32 open_card_poll_bios_event(void) {
    s32 result = CARD_IO_RESULT_PENDING;

    if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE]) == 1)
        result = CARD_IO_RESULT_COMPLETE;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR]) == 1)
        result = CARD_IO_RESULT_ERROR;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT]) == 1)
        result = CARD_IO_RESULT_TIMEOUT;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD]) == 1)
        result = CARD_IO_RESULT_NEW_CARD;
    if (result != CARD_IO_RESULT_PENDING)
        open_card_consume_bios_events();
    return result;
}
