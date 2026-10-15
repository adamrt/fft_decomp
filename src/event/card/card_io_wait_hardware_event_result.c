#include "fft/event_card.h"
#include "fft/main.h"
#include "psx/types.h"

/* Poll the hardware card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * CARD_IO_RESULT_TIMEOUT. */
s32 card_io_wait_hardware_event_result(void) {
    s32 result;
    s32 ready;

    while (1) {
        ready = _card_status(g_card_io_selected_slot) & 1;
        result = card_io_poll_hardware_event_result();
        if (result >= CARD_IO_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return CARD_IO_RESULT_TIMEOUT;
    }
}
