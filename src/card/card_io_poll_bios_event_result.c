#include "fft/main.h"

/* Test the four BIOS card events (done, error, timeout, new card) and return
 * the matching CARD_IO_RESULT_*, or CARD_IO_RESULT_PENDING when none has
 * fired. A result clears all four events. */
s32 card_io_poll_bios_event_result(void) {
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
        card_io_consume_bios_events();
    return result;
}
