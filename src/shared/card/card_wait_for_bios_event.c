#include "fft/main.h"
#include "fft/open.h"
#include "psx/types.h"

/* Block until a BIOS card event fires and return it. Returns timeout when card
 * status bit 0 of the selected slot is set and no event has fired. */
s32 card_wait_for_bios_event(void) {
    s32 card_ready;
    s32 event_result;

    do {
        card_ready = _card_status(g_card_slot) & 1;
        event_result = card_io_poll_bios_event_result();
        if (event_result >= CARD_IO_RESULT_COMPLETE) {
            return event_result;
        }
    } while (card_ready == 0);

    return CARD_IO_RESULT_TIMEOUT;
}
