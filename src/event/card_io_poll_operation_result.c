#include "fft/event_card.h"
#include "psx/types.h"

s32 card_io_poll_operation_result(void) {
    s32 status;
    s32 result;

    if (g_card_io_operation_pending == 0) {
        card_info(g_card_io_selected_slot << 4);
        g_card_io_operation_pending = 1;
    }

    status = card_status(g_card_io_selected_slot) & 1;
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
