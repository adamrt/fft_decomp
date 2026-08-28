#include "fft/card.h"
#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/api.h"
#include "psx/types.h"

/* Poll the four BIOS card events and report which one fired, or pending when
 * none fired. A fired event is consumed along with the rest of the set. */
s32 world_card_poll_bios_events(void) {
    s32 result;

    result = CARD_IO_RESULT_PENDING;
    if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE]) == 1) {
        result = CARD_IO_RESULT_COMPLETE;
    } else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR]) == 1) {
        result = CARD_IO_RESULT_ERROR;
    } else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT]) == 1) {
        result = CARD_IO_RESULT_TIMEOUT;
    } else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD]) == 1) {
        result = CARD_IO_RESULT_NEW_CARD;
    }
    if (result != CARD_IO_RESULT_PENDING) {
        world_card_consume_bios_events();
    }
    return result;
}
