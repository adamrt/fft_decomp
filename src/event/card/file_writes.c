/* Save buffer writes and deletion with retries. */

#include "fft/event_card.h"
#include "psx/libapi.h"
#include "psx/types.h"

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

/* Erase `filename` on the selected memory card slot, trying up to ten times
 * and waiting for the card before each try. Returns erase's nonzero result
 * on success and 0 on failure. */
s32 card_file_delete_selected_with_retries(const char* filename) {
    char path[256];
    /* The retail O0 frame reserves one 32-bit value between the path and result. */
    s32 path_padding;
    s32 result;
    s32 i;

    if (g_card_io_selected_slot == 0) {
        strcpy(path, g_card_file_slot_0_path);
    } else {
        strcpy(path, g_card_file_slot_1_path);
    }
    strcat(path, filename);

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
