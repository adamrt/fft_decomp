#include "fft/card.h"
#include "psx/types.h"

s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin) {
    s32 i;
    /* The target's unoptimized local layout retains one unreferenced word. */
    s32 unused_local;
    s32 result;

    for (i = 0; i < 10; i++) {
        if (card_io_wait_operation_result(10) != CARD_IO_RESULT_COMPLETE)
            return -1;
        result = FileSeek(descriptor, offset, origin);
        if (result != -1)
            break;
    }
    return result;
}
