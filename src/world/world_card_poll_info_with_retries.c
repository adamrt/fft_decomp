#include "fft/world.h"
#include "psx/types.h"

/* Poll _card_info up to count times; return complete on the first successful
 * event, otherwise the last event result (timeout when _card_info failed). */
s32 world_card_poll_info_with_retries(s32 channel, s32 count) {
    s32 result;
    s32 i;

    for (i = 0; i < count; i++) {
        if (_card_info(channel) == 1) {
            result = card_wait_for_bios_event();
            if (result == CARD_IO_RESULT_COMPLETE) {
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
