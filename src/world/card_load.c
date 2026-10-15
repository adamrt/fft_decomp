/* Card info and loads with retries. */

#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* Poll _card_info up to count times; return complete on the first successful
 * event, otherwise the last event result (timeout when _card_info failed). */
s32 card_get_info_with_retries_variant(s32 channel, s32 count) {
    s32 result;
    s32 i;

    for (i = 0; i < count; i++) {
        if (_card_info(channel) == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE) {
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}

/* Retries _card_load for `attempts` frames; returns the BIOS event result or
 * timeout when _card_load does not start. */
s32 world_card_load_with_retries(s32 channel, s32 attempts) {
    s32 i;
    s32 result;

    for (i = 0; i < attempts; i++) {
        s32 status = _card_load(channel);

        result = CARD_IO_RESULT_TIMEOUT;
        if (status == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE) {
                break;
            }
        }
        VSync(2);
    }
    return result;
}
