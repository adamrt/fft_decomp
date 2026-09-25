#include "fft/world.h"
#include "psx/types.h"

/* WORLD twin of card_io_create_new_with_retries (src/event). */
s32 world_card_create_new_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (card_create_new(port) == 1) {
            result = world_card_wait_for_hardware_event();
            if (result >= CARD_IO_RESULT_NEW_CARD)
                result = CARD_IO_RESULT_ERROR;
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
            if (world_card_wait_for_selected_status(1) == CARD_IO_RESULT_COMPLETE) {
                result = CARD_IO_RESULT_COMPLETE;
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
