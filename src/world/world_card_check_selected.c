#include "fft/world.h"
#include "psx/types.h"

/* Issue card_info on the selected slot once, then poll for its result. */
s32 world_card_check_selected(void) {
    s32 busy;
    s32 result;

    if (g_world_card_info_pending == 0) {
        card_info(g_world_card_selected_slot * 0x10);
        g_world_card_info_pending = 1;
    }
    busy = card_status(g_world_card_selected_slot) & 1;
    result = world_card_poll_bios_events();
    if (busy != 0 && result == CARD_IO_RESULT_PENDING) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_world_card_info_pending = 0;
        if (result > CARD_IO_RESULT_COMPLETE) {
            result = world_card_poll_info_with_retries(g_world_card_selected_slot * 0x10, 2);
        }
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
