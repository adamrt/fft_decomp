/* The memory card library's file operations: formatting the selected slot and file open, close, seek, write, delete and
 * read with retries, with each program's own bodies where they diverged. */

#include "fft/event_card.h"
#include "fft/main.h"
#include "fft/world.h"
#include "psx/libapi.h"
#include "psx/types.h"

#ifdef FFT_WORLD
s32 world_card_format_selected_slot(void) {
    char name[8];

    if (card_io_wait_operation_result_variant(10) > CARD_IO_RESULT_COMPLETE) {
        return 0;
    }
    if (g_card_slot == 0) {
        strcpy(name, "bu00:");
    } else {
        strcpy(name, "bu10:");
    }
    format(name);
    format(name);
    return format(name);
}
#endif

#ifdef FFT_WORLD
/* WORLD twin of card_file_open_selected_with_retries (src/event). */
s32 world_card_open_file_with_retries(const char* filename, s32 mode) {
    s32 i;
    s32 result;
    char path[128];

    i = 0;
    if (g_world_card_open_descriptor >= 0) {
        result = card_file_close_with_retries(g_world_card_open_descriptor);
        if (result == 0) {
            return -2;
        }
    }

    if (world_card_get_selected_slot() == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, filename);

    while (i < 40) {
        result = card_io_wait_operation_result_variant(10);
        if (result != CARD_IO_RESULT_COMPLETE) {
            return -2;
        }
        g_world_card_open_descriptor = open(path, mode);
        if (g_world_card_open_descriptor != -1) {
            break;
        }
        i++;
    }
    return g_world_card_open_descriptor;
}
#endif

#ifdef FFT_EVENT_CARD
/* Format the selected memory card, calling format() three times, and return
 * the last call's result; 0 when the card is not ready. */
s32 card_io_format_selected_slot(void) {
    s32 result;
    char path[8];

    result = card_io_wait_operation_result_variant(10);
    if (result > CARD_IO_RESULT_COMPLETE)
        return 0;
    if (g_card_io_selected_slot == 0)
        strcpy(&path[0], g_card_file_slot_0_path);
    else
        strcpy(&path[0], g_card_file_slot_1_path);
    result = format(&path[0]);
    result = format(&path[0]);
    result = format(&path[0]);
    return result;
}
#endif

#ifdef FFT_EVENT_CARD
/*
 * Open `filename` on the selected memory card slot with the BIOS `mode`,
 * closing any file this module still holds open first.
 *
 * Tries up to 40 times, waiting for the card to be ready before each try.
 * Returns the descriptor (also kept in g_card_file_open_descriptor), -1 when
 * every open failed, or -2 when the previous file would not close or the
 * card did not become ready.
 */
s32 card_file_open_selected_with_retries(const char* filename, s32 mode) {
    s32 i;
    s32 result;
    char path[128];

    i = 0;
    if (g_card_file_open_descriptor >= 0) {
        result = card_file_close_with_retries(g_card_file_open_descriptor);
        if (result == 0) {
            return -2;
        }
    }

    if (card_io_get_selected_slot() == 0) {
        strcpy(path, g_card_file_slot_0_path);
    } else {
        strcpy(path, g_card_file_slot_1_path);
    }
    strcat(path, filename);

    while (i < 40) {
        result = card_io_wait_operation_result_variant(10);
        if (result != CARD_IO_RESULT_COMPLETE) {
            return -2;
        }
        g_card_file_open_descriptor = open(path, mode);
        if (g_card_file_open_descriptor != -1) {
            break;
        }
        i++;
    }
    return g_card_file_open_descriptor;
}
#endif

s32 card_file_close_with_retries(s32 descriptor) {
    s32 i;
    s32 wait_success;
    s32 close_result;

    for (i = 0; i < 10; i++) {
        wait_success = card_io_wait_operation_result_variant(10) == CARD_IO_RESULT_COMPLETE;
        close_result = close(descriptor);
        if (close_result == descriptor) {
            break;
        } else {
            wait_success = 0;
        }
    }
    g_card_file_open_descriptor = -1;
    return wait_success;
}

/* lseek with up to ten tries, waiting for the card before each. Returns the
 * new position, or -1 when the card is not ready or every try fails. */
s32 card_file_seek_with_retries(s32 descriptor, s32 offset, s32 origin) {
    s32 i;
    /* The target's unoptimized local layout retains one unreferenced 32-bit value. */
    s32 unused_local;
    s32 result;

    for (i = 0; i < 10; i++) {
        if (card_io_wait_operation_result_variant(10) != CARD_IO_RESULT_COMPLETE)
            return -1;
        result = lseek(descriptor, offset, origin);
        if (result != -1)
            break;
    }
    return result;
}

#ifdef FFT_WORLD
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
#endif

#ifdef FFT_WORLD
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
#endif

#ifdef FFT_WORLD
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
#endif

#ifdef FFT_EVENT_CARD
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
#endif

#ifdef FFT_EVENT_CARD
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
#endif

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
