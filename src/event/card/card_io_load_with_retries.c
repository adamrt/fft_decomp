#include "fft/event_card.h"
#include "psx/types.h"

/* Run _card_load on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times two frames apart. Returns the last result (a
 * rejected call counts as a timeout). */
s32 card_io_load_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_load(port) == 1) {
            result = card_io_wait_bios_event_result();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
        VSync(2);
    }
    return result;
}
