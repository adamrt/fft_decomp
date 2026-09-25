#include "fft/world.h"
#include "psx/api.h"
#include "psx/types.h"

/* WORLD twin of card_file_close_with_retries (src/event). */
s32 world_card_close_file_with_retries(s32 descriptor) {
    s32 i;
    s32 wait_success;
    s32 close_result;

    for (i = 0; i < 10; i++) {
        wait_success = world_card_wait_for_selected_status(10) == CARD_IO_RESULT_COMPLETE;
        close_result = FileClose(descriptor);
        if (close_result == descriptor) {
            break;
        } else {
            wait_success = 0;
        }
    }
    g_world_card_open_descriptor = -1;
    return wait_success;
}
