#include "fft/card.h"
#include "fft/world.h"

/*
 * Write a card file, optionally allocating it first.
 *
 * A failed seek stops the write; partial writes and -1 results allow another
 * attempt, up to 40 write attempts. A successful close returns bytes written.
 */
s32 world_card_write_buffer_to_file(const char* filename, const u8* source, u32 size, s32 create_file) {
    u32 total_written;
    s32 result;
    s32 retries;

    if (create_file != 0) {
        if (world_card_open_file_with_retries(
                filename, ((u32)((const world_card_file_header_t*)source)->allocation_blocks << 16) | 0x200)
            < 0) {
            return -1;
        }
        if (world_card_close_file_with_retries(g_world_card_open_descriptor) == 0) {
            return -1;
        }
    }
    result = world_card_open_file_with_retries(filename, 2);
    if (result < 0) {
        return -1;
    }
    if (FileGetError(g_world_card_open_descriptor) != 0) {
        world_card_close_file_with_retries(g_world_card_open_descriptor);
        return -1;
    }
    total_written = 0;
    for (retries = 0; retries < 40; retries++) {
        if (world_card_seek_file_with_retries(g_world_card_open_descriptor, total_written, 1) < 0) {
            break;
        }
        result = FileWrite(g_world_card_open_descriptor, source + total_written, size - total_written);
        if (result != -1) {
            total_written += result;
            if (total_written >= size) {
                break;
            }
        }
    }
    if (world_card_close_file_with_retries(g_world_card_open_descriptor) == 0) {
        return -1;
    }
    /* The CARD twin retains this ineffective result check. GCC removes it
     * after allocation; removing it here also removes the target's s1 saves. */
    if (result != size) {
        result = -1;
    }
    return total_written;
}
