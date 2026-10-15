/* Memory card file close and seek, each retried up to ten times. */

#include "fft/event_card.h"
#include "fft/main.h"
#include "psx/libapi.h"

s32 card_file_close_with_retries(s32 descriptor) {
    s32 i;
    s32 wait_success;
    s32 close_result;

    for (i = 0; i < 10; i++) {
        wait_success = card_io_wait_operation_result_variant(10) == CARD_IO_RESULT_COMPLETE;
        close_result = close(descriptor);
        if (close_result == descriptor) {
            break;
        } else {
            wait_success = 0;
        }
    }
    g_card_file_open_descriptor = -1;
    return wait_success;
}

/* lseek with up to ten tries, waiting for the card before each. Returns the
 * new position, or -1 when the card is not ready or every try fails. */
s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin) {
    s32 i;
    /* The target's unoptimized local layout retains one unreferenced 32-bit value. */
    s32 unused_local;
    s32 result;

    for (i = 0; i < 10; i++) {
        if (card_io_wait_operation_result_variant(10) != CARD_IO_RESULT_COMPLETE)
            return -1;
        result = lseek(descriptor, offset, origin);
        if (result != -1)
            break;
    }
    return result;
}
