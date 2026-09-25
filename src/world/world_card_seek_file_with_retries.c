#include "fft/world.h"
#include "psx/types.h"

/* Retries FileSeek up to ten times; gives up with -1 when
 * world_card_wait_for_selected_status reports a problem. */
s32 world_card_seek_file_with_retries(s32 fd, s32 offset, s32 mode) {
    s32 i;
    s32 result;

    for (i = 0; i < 10; i++) {
        if (world_card_wait_for_selected_status(10) != CARD_IO_RESULT_COMPLETE) {
            return -1;
        }
        result = FileSeek(fd, offset, mode);
        if (result != -1) {
            break;
        }
    }
    return result;
}
