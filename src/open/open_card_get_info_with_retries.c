#include "fft/open.h"
#include "psx/types.h"

/* Issue _card_info on `port` up to retry_limit times, waiting for its BIOS event
 * each time. Returns complete on the first success, otherwise the result of the
 * last attempt (error when _card_info itself refused). */
s32 open_card_get_info_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_info(port) == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_ERROR;
        }
    }
    return result;
}
