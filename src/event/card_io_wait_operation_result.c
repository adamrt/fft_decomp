#include "fft/card.h"
#include "psx/types.h"

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
