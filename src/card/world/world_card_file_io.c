/* Card file write, delete and chunked read with retries. */

#include "fft/world.h"
#include "psx/libapi.h"
#include "psx/types.h"

/*
 * Write a card file, optionally allocating it first.
 *
 * A failed seek stops the write; partial writes and -1 results allow another
 * attempt, up to 40 write attempts. A successful close returns bytes written.
 */
s32 world_card_write_buffer_to_file(const char* filename, const card_save_buffer_t* source, u32 size, s32 create_file) {
    u32 total_written;
    s32 result;
    s32 retries;

    if (create_file != 0) {
        if (world_card_open_file_with_retries(filename, (source->header.block_count << 16) | 0x200) < 0) {
            return -1;
        }
        if (card_file_close_with_retries(g_world_card_open_descriptor) == 0) {
            return -1;
        }
    }
    result = world_card_open_file_with_retries(filename, 2);
    if (result < 0) {
        return -1;
    }
    if (_get_error(g_world_card_open_descriptor) != 0) {
        card_file_close_with_retries(g_world_card_open_descriptor);
        return -1;
    }
    total_written = 0;
    for (retries = 0; retries < 40; retries++) {
        if (card_file_seek_with_retries(g_world_card_open_descriptor, total_written, 1) < 0) {
            break;
        }
        result = write(g_world_card_open_descriptor, (const u8*)source + total_written, size - total_written);
        if (result != -1) {
            total_written += result;
            if (total_written >= size) {
                break;
            }
        }
    }
    if (card_file_close_with_retries(g_world_card_open_descriptor) == 0) {
        return -1;
    }
    /* The CARD twin retains this ineffective result check. GCC removes it
     * after allocation; removing it here also removes the target's s1 saves. */
    if (result != size) {
        result = -1;
    }
    return total_written;
}

/* Deletes `name` from the selected memory card, retrying erase up to
 * ten times; gives up with 0 when world_card_wait_for_selected_status reports a problem. */
s32 world_card_delete_file(const char* name) {
    char path[256];
    s32 i;
    s32 result;

    if (g_card_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, name);
    for (i = 0; i < 10; i++) {
        if (card_io_wait_operation_result_variant(10) != CARD_IO_RESULT_COMPLETE) {
            return 0;
        }
        result = erase(path);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

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
        if (card_file_seek_with_retries(g_world_card_open_descriptor, total, 1) < 0) {
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
