#include "fft/main.h"
#include "fft/world.h"
#include "psx/types.h"

/* Issue _card_info on the selected slot once, then poll for its result. */
s32 card_poll_info_result_variant(void) {
    s32 busy;
    s32 result;

    if (g_card_info_pending == 0) {
        _card_info(g_card_slot * 0x10);
        g_card_info_pending = 1;
    }
    busy = _card_status(g_card_slot) & 1;
    result = card_io_poll_bios_event_result();
    if (busy != 0 && result == CARD_IO_RESULT_PENDING) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_card_info_pending = 0;
        if (result > CARD_IO_RESULT_COMPLETE) {
            result = card_get_info_with_retries_variant(g_card_slot * 0x10, 2);
        }
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
