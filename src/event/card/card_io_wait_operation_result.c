#include "fft/event_card.h"
#include "psx/types.h"

/* Run the card status poll to a result, repeating it up to `retry_limit`
 * times until it reports completion. Returns the last result. */
s32 card_io_wait_operation_result(s32 retry_limit) {
    s32 result;
    s32 i = 0;

    while (i < retry_limit) {
        do {
            result = card_io_poll_operation_result();
        } while (result == CARD_IO_RESULT_PENDING);
        if (result == CARD_IO_RESULT_COMPLETE)
            break;
        i++;
    }
    return result;
}
