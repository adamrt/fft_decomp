#include "fft/event_card.h"
#include "psx/types.h"

s32 card_file_close_with_retries(s32 descriptor) {
    s32 i;
    s32 wait_success;
    s32 close_result;

    for (i = 0; i < 10; i++) {
        wait_success = card_io_wait_operation_result(10) == CARD_IO_RESULT_COMPLETE;
        close_result = FileClose(descriptor);
        if (close_result == descriptor) {
            break;
        } else {
            wait_success = 0;
        }
    }
    g_card_file_open_descriptor = -1;
    return wait_success;
}
