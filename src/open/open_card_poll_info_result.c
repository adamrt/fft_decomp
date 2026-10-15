#include "fft/main.h"
#include "fft/open.h"
#include "psx/types.h"

/* Issue _card_info on the selected slot once, then poll for its result; the
 * request is issued again when card status bit 0 is set with no event. A result
 * other than complete is checked again with two blocking attempts, and error is
 * reported as timeout. */
s32 open_card_poll_info_result(void) {
    s32 status;
    s32 result;

    if (g_open_card_info_pending == 0) {
        _card_info(g_card_slot << 4);
        g_open_card_info_pending = 1;
    }

    status = _card_status(g_card_slot) & 1;
    result = card_io_poll_bios_event_result();
    if (status != 0 && result == CARD_IO_RESULT_PENDING) {
        g_open_card_info_pending = 0;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_open_card_info_pending = 0;
    }
    if (result > CARD_IO_RESULT_COMPLETE) {
        result = open_card_get_info_with_retries(g_card_slot << 4, 2);
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
