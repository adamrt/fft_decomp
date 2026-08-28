#include "fft/card.h"
#include "fft/open.h"
#include "psx/types.h"

s32 open_card_wait_for_bios_event(void) {
    s32 card_ready;
    s32 event_result;

    do {
        card_ready = card_status(g_open_card_slot) & 1;
        event_result = open_card_poll_bios_event();
        if (event_result >= CARD_IO_RESULT_COMPLETE) {
            return event_result;
        }
    } while (card_ready == 0);

    return CARD_IO_RESULT_TIMEOUT;
}
