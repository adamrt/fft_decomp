#include "fft/card.h"
#include "fft/world.h"
#include "psx/types.h"

/* Check the selected memory card and normalize failures to timeout. */
s32 world_card_probe_selected(void) {
    s32 result = world_card_wait_for_selected_status(10);

    if (result == CARD_IO_RESULT_NEW_CARD) {
        if (world_card_create_new_with_retries(g_world_card_selected_slot * 0x10, 10) != CARD_IO_RESULT_COMPLETE) {
            return CARD_IO_RESULT_TIMEOUT;
        }
        result = world_card_load_with_retries(g_world_card_selected_slot * 0x10, 0x1e);
    } else if (result == CARD_IO_RESULT_COMPLETE) {
        result = world_card_load_with_retries(g_world_card_selected_slot * 0x10, 0x1e);
    }
    if (result == CARD_IO_RESULT_NEW_CARD) {
        return CARD_IO_RESULT_NEW_CARD;
    }
    if (result != CARD_IO_RESULT_COMPLETE) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
