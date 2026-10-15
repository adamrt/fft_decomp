#include "fft/world.h"
#include "psx/libetc.h"
#include "psx/types.h"

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
