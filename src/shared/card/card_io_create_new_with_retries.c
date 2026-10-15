#include "fft/event_card.h"
#include "fft/main.h"
#include "psx/libapi.h"

/* Clear the "new card" state of `port` with _card_clear, trying up to
 * `retry_limit` times. Returns CARD_IO_RESULT_COMPLETE once the hardware
 * event or a follow-up info request reports completion, otherwise the last
 * failure (a rejected call counts as a timeout). */
s32 card_io_create_new_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_clear(port) == 1) {
            result = card_wait_for_hardware_event_variant();
            if (result >= CARD_IO_RESULT_NEW_CARD)
                result = CARD_IO_RESULT_ERROR;
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
            if (card_io_wait_operation_result(1) == CARD_IO_RESULT_COMPLETE) {
                result = CARD_IO_RESULT_COMPLETE;
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
