#include "fft/world.h"
#include "psx/types.h"

/* Poll the selected memory card until it reports a settled state.
 *
 * Each attempt retries while the result is pending. Returns the first
 * non-complete status, or the last status once `attempts` settled polls have
 * completed. */
s32 card_io_wait_operation_result_variant(s32 attempts) {
    s32 i;
    s32 status;

    for (i = 0; i < attempts; i++) {
        do {
            status = card_poll_info_result_variant();
        } while (status == CARD_IO_RESULT_PENDING);
        if (status == CARD_IO_RESULT_COMPLETE) {
            break;
        }
    }
    return status;
}
