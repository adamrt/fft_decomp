#include "fft/open.h"
#include "psx/types.h"

s32 open_card_poll_info_result(void) {
    s32 status;
    s32 result;

    if (g_open_card_info_pending == 0) {
        card_info(g_open_card_slot << 4);
        g_open_card_info_pending = 1;
    }

    status = card_status(g_open_card_slot) & 1;
    result = open_card_poll_bios_event();
    if (status != 0 && result == CARD_IO_RESULT_PENDING) {
        g_open_card_info_pending = 0;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_open_card_info_pending = 0;
    }
    if (result > CARD_IO_RESULT_COMPLETE) {
        result = open_card_get_info_with_retries(g_open_card_slot << 4, 2);
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
