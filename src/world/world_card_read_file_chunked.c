#include "fft/world.h"
#include "psx/libapi.h"
#include "psx/types.h"

/*
 * Read a card file in up to 40 attempts.
 *
 * A failed seek stops the read; partial reads and -1 results allow another
 * attempt. A successful close returns the bytes read.
 */
s32 world_card_read_file_chunked(const char* filename, u8* destination, u32 size) {
    u32 total;
    s32 result;
    s32 i;

    result = world_card_open_file_with_retries(filename, 1);
    if (result < 0) {
        return -1;
    }
    if (_get_error(g_world_card_open_descriptor) != 0) {
        card_file_close_with_retries(g_world_card_open_descriptor);
        return -1;
    }
    total = 0;
    for (i = 0; i < 0x28; i++) {
        if (world_card_seek_file_with_retries(g_world_card_open_descriptor, total, 1) < 0) {
            break;
        }
        result = read(g_world_card_open_descriptor, destination + total, size - total);
        if (result != -1) {
            total += result;
            if (total >= size) {
                break;
            }
        }
    }
    if (card_file_close_with_retries(g_world_card_open_descriptor) == 0) {
        return -1;
    }
    /* Ineffective check retained from the CARD twin: it emits nothing but
     * keeps `result` live across the calls, hence its callee-saved register. */
    if (result != size) {
        result = -1;
    }
    return total;
}
