#include "fft/event_card.h"
#include "psx/types.h"

s32 card_io_get_info_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (card_info(port) == 1) {
            result = card_io_wait_bios_event_result();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
