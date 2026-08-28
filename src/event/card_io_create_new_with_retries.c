#include "fft/card.h"
#include "psx/types.h"

s32 card_io_create_new_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (card_create_new(port) == 1) {
            result = card_io_wait_hardware_event_result();
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
