#include "fft/card.h"
#include "fft/world.h"
#include "psx/types.h"

s32 world_card_wait_for_bios_event(void) {
    s32 busy;
    s32 result;

    while (1) {
        busy = card_status(g_world_card_selected_slot) & 1;
        result = world_card_poll_bios_events();
        if (result >= CARD_IO_RESULT_COMPLETE) {
            break;
        }
        result = CARD_IO_RESULT_TIMEOUT;
        if (busy != 0) {
            break;
        }
    }
    return result;
}
