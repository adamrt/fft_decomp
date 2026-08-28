#include "fft/card.h"
#include "psx/types.h"

s32 open_card_get_info_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (card_info(port) == 1) {
            result = open_card_wait_for_bios_event();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_ERROR;
        }
    }
    return result;
}
