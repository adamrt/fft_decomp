#include "fft/event_card.h"
#include "psx/types.h"

/*
 * Write `size` bytes of `source` to the memory card file `filename` and
 * return the number of bytes written, or -1 when the file cannot be opened
 * or closed.
 *
 * With `create_file` set the file is first created, with the block count
 * taken from its save header, closed and reopened for writing. The write
 * seeks by the running total, with origin 1, before each attempt and gives up
 * after 40 attempts that leave it short. The final comparison of the last
 * write against `size` changes only a value that is not returned.
 */
s32 card_file_write_buffer(const char* filename, const card_save_buffer_t* source, s32 size, s32 create_file) {
    s32 error;
    s32 result;
    s32 retries;
    s32 total_written;

    if (create_file != 0) {
        /* Taking the field address preserves the unoptimized creation-mode load. */
        result = card_file_open_selected_with_retries(filename, (*(&source->header.block_count) << 16) | 0x200);
        if (result <= -1) {
            return -1;
        } else {
            if (card_file_close_with_retries(g_card_file_open_descriptor) == 0) {
                return -1;
            }
            result = card_file_open_selected_with_retries(filename, 2);
            if (result <= -1) {
                return -1;
            }
        }
        create_file = 0;
    } else {
        result = card_file_open_selected_with_retries(filename, 2);
        if (result < 0) {
            return -1;
        }
    }

    error = _get_error(g_card_file_open_descriptor);
    if (error != 0) {
        card_file_close_with_retries(g_card_file_open_descriptor);
        return -1;
    }

    total_written = 0;
    retries = 0;
    while (retries < 40) {
        if (card_file_seek_with_retries(g_card_file_open_descriptor, total_written, 1) < 0) {
            break;
        }

        result = write(g_card_file_open_descriptor, (const u8*)source + total_written, size - total_written);
        if (result != -1) {
            total_written += result;
            if (total_written >= (u32)size) {
                break;
            }
        }
        retries++;
    }

    if (card_file_close_with_retries(g_card_file_open_descriptor) == 0) {
        return -1;
    }
    if (result != size) {
        result = -1;
    }
    return total_written;
}
