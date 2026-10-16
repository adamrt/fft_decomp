/* The memory card library: slot selection, BIOS and hardware event polling, info, load and create with retries,
 * operation result polling, save file enumeration and free blocks, formatting, and file open, close, seek, write,
 * delete and read with retries. The CARD overlay compiles it unoptimized and WORLD optimized from the same source;
 * the opening links only the slot and event routines and keeps its own info and polling bodies. */

#include "fft/card.h"
#include "fft/main.h"
#include "psx/kernel.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/types.h"

static void card_io_consume_bios_events(void);
static void card_io_consume_hardware_events(void);

void card_io_set_selected_slot(s32 slot) {
    g_card_slot = slot;
}

#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD)
static s32 card_io_get_selected_slot(void) {
    return g_card_slot;
}
#endif

#ifdef FFT_WORLD
/* Empty. Called before a soft reset and when the memory card screen closes. */
void world_card_close_stub(void) {
}
#endif

void card_io_consume_all_events(void) {
    card_io_consume_bios_events();
    card_io_consume_hardware_events();
}

/* Test the four BIOS card events (done, error, timeout, new card) and return
 * the matching CARD_IO_RESULT_*, or CARD_IO_RESULT_PENDING when none has
 * fired. A result clears all four events. */
s32 card_io_poll_bios_event_result(void) {
    s32 result = CARD_IO_RESULT_PENDING;
    if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE]) == 1)
        result = CARD_IO_RESULT_COMPLETE;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR]) == 1)
        result = CARD_IO_RESULT_ERROR;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT]) == 1)
        result = CARD_IO_RESULT_TIMEOUT;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD]) == 1)
        result = CARD_IO_RESULT_NEW_CARD;
    if (result != CARD_IO_RESULT_PENDING)
        card_io_consume_bios_events();
    return result;
}

/* Poll the BIOS card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * CARD_IO_RESULT_TIMEOUT. */
s32 card_io_wait_for_bios_event(void) {
    s32 result;
    s32 ready;

    while (1) {
        ready = _card_status(g_card_slot) & 1;
        result = card_io_poll_bios_event_result();
        if (result >= CARD_IO_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return CARD_IO_RESULT_TIMEOUT;
    }
}

static void card_io_consume_bios_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD]);
}

/* Test the four hardware card events (done, error, timeout, new card) and return
 * the matching CARD_IO_RESULT_*, or CARD_IO_RESULT_PENDING when none has
 * fired. A result clears all four events. */
static s32 card_io_poll_hardware_event_result(void) {
    s32 result = CARD_IO_RESULT_PENDING;
    if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE]) == 1)
        result = CARD_IO_RESULT_COMPLETE;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR]) == 1)
        result = CARD_IO_RESULT_ERROR;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT]) == 1)
        result = CARD_IO_RESULT_TIMEOUT;
    else if (TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD]) == 1)
        result = CARD_IO_RESULT_NEW_CARD;
    if (result != CARD_IO_RESULT_PENDING)
        card_io_consume_hardware_events();
    return result;
}

/* Poll the hardware card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * CARD_IO_RESULT_TIMEOUT. Nothing in OPEN.BIN calls it. */
static s32 card_io_wait_for_hardware_event(void) {
    s32 result;
    s32 ready;

    while (1) {
        ready = _card_status(g_card_slot) & 1;
        result = card_io_poll_hardware_event_result();
        if (result >= CARD_IO_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return CARD_IO_RESULT_TIMEOUT;
    }
}

/* Consume the four hardware-card event notifications in handle order. */
static void card_io_consume_hardware_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD]);
}

#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD)
/* Run _card_info on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times. Returns the last result (a rejected call counts
 * as a timeout; the opening's own body counts it as an error). */
s32 card_io_get_info_with_retries_variant(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_info(port) == 1) {
            result = card_io_wait_for_bios_event();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}

/* Run _card_load on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times two frames apart. Returns the last result (a
 * rejected call counts as a timeout). */
static s32 card_io_load_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_load(port) == 1) {
            result = card_io_wait_for_bios_event();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
        VSync(2);
    }
    return result;
}

/* Clear the "new card" state of `port` with _card_clear, trying up to
 * `retry_limit` times. Returns CARD_IO_RESULT_COMPLETE once the hardware
 * event or a follow-up info request reports completion, otherwise the last
 * failure (a rejected call counts as a timeout). */
static s32 card_io_create_new_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_clear(port) == 1) {
            result = card_io_wait_for_hardware_event();
            if (result >= CARD_IO_RESULT_NEW_CARD)
                result = CARD_IO_RESULT_ERROR;
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
            if (card_io_wait_operation_result(1) == CARD_IO_RESULT_COMPLETE) {
                result = CARD_IO_RESULT_COMPLETE;
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}

/* Poll the selected card slot: start a _card_info request when none is
 * pending and return its outcome (CARD_IO_RESULT_PENDING while it runs).
 * A set bit 0 of _card_status with no event yet counts as a timeout. A result
 * other than "complete" is checked again with a retried info request, and an
 * error is reported as a timeout. The opening's own body reissues the request
 * instead of timing out. */
s32 card_io_poll_operation_result_variant(void) {
    s32 status;
    s32 result;

    if (g_card_info_pending == 0) {
        _card_info(g_card_slot << 4);
        g_card_info_pending = 1;
    }

    status = _card_status(g_card_slot) & 1;
    result = card_io_poll_bios_event_result();
    if (status != 0 && result == CARD_IO_RESULT_PENDING) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_card_info_pending = 0;
    }
    if (result > CARD_IO_RESULT_COMPLETE) {
        result = card_io_get_info_with_retries_variant(g_card_slot << 4, 2);
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}

/* Run the card status poll to a result, repeating it up to `retry_limit`
 * times until it reports completion. Returns the last result. */
s32 card_io_wait_operation_result(s32 retry_limit) {
    s32 result;
    s32 i = 0;

    while (i < retry_limit) {
        do {
            result = card_io_poll_operation_result_variant();
        } while (result == CARD_IO_RESULT_PENDING);
        if (result == CARD_IO_RESULT_COMPLETE)
            break;
        i++;
    }
    return result;
}

/* Count the files of the selected slot into `entries`: the game's saves, or
 * every file when `all_files` is set. */
s32 card_file_enumerate_saves(DIRENTRY* entries, s32 all_files) {
    s32 count;
    char path[128];

    if (g_card_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    if (all_files == 0) {
        strcat(path, "BISLPS-00700FF7*");
    } else {
        strcat(path, "*");
    }

    count = 0;
    if (firstfile(path, entries) == entries) {
        do {
            count++;
            entries++;
        } while (nextfile(entries) == entries);
    }
    return count;
}

/* Free blocks on a 15-block card given its directory: each file takes its
 * size rounded up to 8192-byte blocks. */
s32 card_file_count_free_blocks(const DIRENTRY* entries, s32 entry_count) {
    s32 i;
    s32 used_blocks;

    used_blocks = 0;
    for (i = 0; i < entry_count; i++) {
        used_blocks += entries[i].size >> 13;
        used_blocks += !!(entries[i].size % 0x2000);
    }

    if (used_blocks >= 16) {
        return 0;
    }
    return 15 - used_blocks;
}

/* Bring the selected card to a readable state: acknowledge a newly inserted
 * card, then load its directory. Returns CARD_IO_RESULT_COMPLETE,
 * CARD_IO_RESULT_NEW_CARD (the load still reports a new, unformatted card)
 * or CARD_IO_RESULT_TIMEOUT for any failure. */
s32 card_io_open_or_create_selected_card(void) {
    s32 result = card_io_wait_operation_result(10);

    if (result == CARD_IO_RESULT_NEW_CARD) {
        result = card_io_create_new_with_retries(g_card_slot << 4, 10);
        if (result != CARD_IO_RESULT_COMPLETE)
            return CARD_IO_RESULT_TIMEOUT;
    }
    if (result == CARD_IO_RESULT_COMPLETE)
        result = card_io_load_with_retries(g_card_slot << 4, 30);
    if (result == CARD_IO_RESULT_NEW_CARD)
        return CARD_IO_RESULT_NEW_CARD;
    if (result != CARD_IO_RESULT_COMPLETE)
        result = CARD_IO_RESULT_TIMEOUT;
    return result;
}

/* Format the selected memory card, calling format() three times, and return
 * the last call's result; 0 when the card is not ready. */
s32 card_io_format_selected_slot(void) {
    s32 result;
    char path[8];

    result = card_io_wait_operation_result(10);
    if (result > CARD_IO_RESULT_COMPLETE)
        return 0;
    if (g_card_slot == 0)
        strcpy(&path[0], "bu00:");
    else
        strcpy(&path[0], "bu10:");
    result = format(&path[0]);
    result = format(&path[0]);
    result = format(&path[0]);
    return result;
}

/* Open `filename` on the selected memory card slot with the BIOS `mode`,
 * closing any file still held open first. Tries up to 40 times, waiting for
 * the card to be ready before each try. Returns the descriptor (also kept in
 * g_card_file_open_descriptor), -1 when every open failed, or -2 when the
 * previous file would not close or the card did not become ready. */
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
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, filename);

    while (i < 40) {
        result = card_io_wait_operation_result(10);
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

s32 card_file_close_with_retries(s32 descriptor) {
    s32 i;
    s32 wait_success;
    s32 close_result;

    for (i = 0; i < 10; i++) {
        wait_success = card_io_wait_operation_result(10) == CARD_IO_RESULT_COMPLETE;
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
    /* The unoptimized local layout retains one unreferenced 32-bit value. */
    s32 unused_local;
    s32 result;

    for (i = 0; i < 10; i++) {
        if (card_io_wait_operation_result(10) != CARD_IO_RESULT_COMPLETE)
            return -1;
        result = lseek(descriptor, offset, origin);
        if (result != -1)
            break;
    }
    return result;
}

/* Write `size` bytes of `source` to `filename` on the selected slot, creating
 * the file first when `create_file` is set, in up to 40 chunks. Returns the
 * bytes written, or -1 when an open, close or error check fails. */
s32 card_file_write_buffer(const char* filename, const card_save_buffer_t* source, u32 size, s32 create_file) {
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
            if (total_written >= size) {
                break;
            }
        }
        retries++;
    }

    if (card_file_close_with_retries(g_card_file_open_descriptor) == 0) {
        return -1;
    }
    /* This result check has no effect; it keeps `result` live across the
     * calls, which the original's callee-saved register shows. */
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
    /* The unoptimized frame reserves one 32-bit value between the path and result. */
    s32 path_padding;
    s32 result;
    s32 i;

    if (g_card_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, filename);

    for (i = 0; i < 10; i++) {
        if (card_io_wait_operation_result(10) != CARD_IO_RESULT_COMPLETE) {
            return 0;
        }
        result = erase(path);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

#    ifdef FFT_WORLD
/* Read a card file in up to 40 attempts. A failed seek stops the read;
 * partial reads and -1 results allow another attempt. A successful close
 * returns the bytes read. */
s32 world_card_read_file_chunked(const char* filename, u8* destination, u32 size) {
    u32 total;
    s32 result;
    s32 i;

    result = card_file_open_selected_with_retries(filename, 1);
    if (result < 0) {
        return -1;
    }
    if (_get_error(g_card_file_open_descriptor) != 0) {
        card_file_close_with_retries(g_card_file_open_descriptor);
        return -1;
    }
    total = 0;
    for (i = 0; i < 0x28; i++) {
        if (card_file_seek_with_retries(g_card_file_open_descriptor, total, 1) < 0) {
            break;
        }
        result = read(g_card_file_open_descriptor, destination + total, size - total);
        if (result != -1) {
            total += result;
            if (total >= size) {
                break;
            }
        }
    }
    if (card_file_close_with_retries(g_card_file_open_descriptor) == 0) {
        return -1;
    }
    /* This result check has no effect; it keeps `result` live across the
     * calls, which the original's callee-saved register shows. */
    if (result != size) {
        result = -1;
    }
    return total;
}
#    endif

/* Read `size` bytes at the file's current position, seeking back and trying
 * again after a short transfer, up to ten tries in all. Returns `size`, or -1
 * when the position cannot be read, the descriptor reports an error or every
 * try falls short. */
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
 * again after a short transfer, up to ten tries in all. Returns `size`, or -1
 * when the position cannot be read, the descriptor reports an error or every
 * try falls short. */
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
#endif
