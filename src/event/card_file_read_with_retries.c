#include "fft/event_card.h"
#include "psx/types.h"

s32 card_file_read_with_retries(s32 descriptor, void* destination, s32 size) {
    s32 error;
    s32 result;
    s32 i;
    s32 initial_position;

    initial_position = card_file_seek_with_retries(descriptor, 0, 1);
    if (initial_position < 0) {
        return -1;
    }
    error = FileGetError(descriptor);
    if (error != 0) {
        return -1;
    }

    for (i = 0; i < 10; i++) {
        result = FileRead(descriptor, destination, size);
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
