/* Memory card file read and write, each retried up to ten times after a short transfer. */

#include "fft/event_card.h"
#include "psx/libapi.h"

s32 card_file_read_with_retries(s32 descriptor, void* destination, s32 size) {
    s32 error;
    s32 result;
    s32 i;
    s32 initial_position;

    initial_position = card_file_seek_with_retries(descriptor, 0, 1);
    if (initial_position < 0) {
        return -1;
    }
    error = _get_error(descriptor);
    if (error != 0) {
        return -1;
    }

    for (i = 0; i < 10; i++) {
        result = read(descriptor, destination, size);
        if (result == size) {
            break;
        }
        if (card_file_seek_with_retries(descriptor, initial_position, 0) < 0) {
            break;
        }
    }
    if (result != size) {
        result = -1;
    }
    return result;
}

/* Write `size` bytes at the file's current position, seeking back and trying
 * again after a short transfer, up to ten tries in all. Returns `size`, or -1 when
 * the position cannot be read, the descriptor reports an error or every try
 * falls short. */
s32 card_file_write_with_retries(s32 descriptor, const void* source, s32 size) {
    s32 error;
    s32 result;
    s32 i;
    s32 initial_position;

    initial_position = card_file_seek_with_retries(descriptor, 0, 1);
    if (initial_position < 0) {
        return -1;
    }
    error = _get_error(descriptor);
    if (error != 0) {
        return -1;
    }

    for (i = 0; i < 10; i++) {
        result = write(descriptor, source, size);
        if (result == size) {
            break;
        }
        if (card_file_seek_with_retries(descriptor, initial_position, 0) < 0) {
            break;
        }
    }
    if (result != size) {
        result = -1;
    }
    return result;
}
