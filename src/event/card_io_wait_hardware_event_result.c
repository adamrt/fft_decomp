#include "fft/card.h"
#include "psx/types.h"

s32 card_io_wait_hardware_event_result(void) {
    s32 result;
    s32 ready;

    while (1) {
        ready = card_status(g_card_io_selected_slot) & 1;
        result = card_io_poll_hardware_event_result();
        if (result >= CARD_IO_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return CARD_IO_RESULT_TIMEOUT;
    }
}
