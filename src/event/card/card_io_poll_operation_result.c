#include "fft/event_card.h"
#include "fft/main.h"
#include "psx/types.h"

/*
 * Poll the selected card slot: start a _card_info request when none is
 * pending and return its outcome (CARD_IO_RESULT_PENDING while it runs).
 *
 * A set bit 0 of _card_status with no event yet counts as a timeout. A result other than
 * "complete" is checked again with a retried info request, and an error is
 * reported as a timeout.
 */
s32 card_io_poll_operation_result(void) {
    s32 status;
    s32 result;

    if (g_card_io_operation_pending == 0) {
        _card_info(g_card_io_selected_slot << 4);
        g_card_io_operation_pending = 1;
    }

    status = _card_status(g_card_io_selected_slot) & 1;
    result = card_io_poll_bios_event_result();
    if (status != 0 && result == CARD_IO_RESULT_PENDING) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_card_io_operation_pending = 0;
    }
    if (result > CARD_IO_RESULT_COMPLETE) {
        result = card_io_get_info_with_retries(g_card_io_selected_slot << 4, 2);
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
