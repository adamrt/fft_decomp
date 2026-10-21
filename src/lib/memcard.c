/* Memory-card sessions and save-file I/O. CARD and WORLD compile this shared
 * implementation with different optimization profiles; OPEN uses the event
 * helpers and supplies its own card-status polling bodies. */

#include "fft/memcard.h"
#include "fft/save.h"
#include "psx/kernel.h"
#include "psx/libapi.h"
#include "psx/libc.h"
#include "psx/libetc.h"
#include "psx/types.h"

/* OPEN only uses the card-status helpers; WORLD and CARD also open save files. */
#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD)
static s32 s_file_descriptor = -1;
#endif

static void bios_events_clear(void);
static void hardware_events_clear(void);

/* Choose the memory card slot (0 or 1) that later card operations use. */
void memcard_slot_set(s32 slot) {
    g_memcard_slot = slot;
}

#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD)
/* The selected memory card slot, which picks the bu00: or bu10: device. */
static s32 slot_get(void) {
    return g_memcard_slot;
}
#endif

#ifdef FFT_WORLD
/* Empty. Called before a soft reset and when the memory card screen closes. */
void save_close_stub(void) {
}
#endif

/* Clear every pending BIOS and hardware card event before a card session starts. */
void memcard_events_clear(void) {
    bios_events_clear();
    hardware_events_clear();
}

/* Test the four BIOS card events (done, error, timeout, new card) and return
 * the matching MEMCARD_RESULT_*, or MEMCARD_RESULT_PENDING when none has
 * fired. A result clears all four events. */
memcard_result_e memcard_bios_event_poll(void) {
    memcard_result_e result = MEMCARD_RESULT_PENDING;
    if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_DONE]) == 1)
        result = MEMCARD_RESULT_COMPLETE;
    else if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_ERROR]) == 1)
        result = MEMCARD_RESULT_ERROR;
    else if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_TIMEOUT]) == 1)
        result = MEMCARD_RESULT_TIMEOUT;
    else if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_NEW_CARD]) == 1)
        result = MEMCARD_RESULT_NEW_CARD;
    if (result != MEMCARD_RESULT_PENDING)
        bios_events_clear();
    return result;
}

/* Poll the BIOS card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * MEMCARD_RESULT_TIMEOUT. */
memcard_result_e memcard_bios_event_wait(void) {
    memcard_result_e result;
    s32 ready;

    while (1) {
        ready = _card_status(g_memcard_slot) & 1;
        result = memcard_bios_event_poll();
        if (result >= MEMCARD_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return MEMCARD_RESULT_TIMEOUT;
    }
}

/* Clear the four BIOS card events (done, error, timeout, new card) by testing each. */
static void bios_events_clear(void) {
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_DONE]);
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_ERROR]);
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_TIMEOUT]);
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_BIOS_NEW_CARD]);
}

/* Test the four hardware card events (done, error, timeout, new card) and return
 * the matching MEMCARD_RESULT_*, or MEMCARD_RESULT_PENDING when none has
 * fired. A result clears all four events. */
static memcard_result_e hardware_event_poll(void) {
    memcard_result_e result = MEMCARD_RESULT_PENDING;
    if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_DONE]) == 1)
        result = MEMCARD_RESULT_COMPLETE;
    else if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_ERROR]) == 1)
        result = MEMCARD_RESULT_ERROR;
    else if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_TIMEOUT]) == 1)
        result = MEMCARD_RESULT_TIMEOUT;
    else if (TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_NEW_CARD]) == 1)
        result = MEMCARD_RESULT_NEW_CARD;
    if (result != MEMCARD_RESULT_PENDING)
        hardware_events_clear();
    return result;
}

/* Poll the hardware card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * MEMCARD_RESULT_TIMEOUT. Nothing in OPEN.BIN calls it. */
static memcard_result_e hardware_event_wait(void) {
    memcard_result_e result;
    s32 ready;

    while (1) {
        ready = _card_status(g_memcard_slot) & 1;
        result = hardware_event_poll();
        if (result >= MEMCARD_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return MEMCARD_RESULT_TIMEOUT;
    }
}

/* Consume the four hardware-card event notifications in handle order. */
static void hardware_events_clear(void) {
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_DONE]);
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_ERROR]);
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_TIMEOUT]);
    TestEvent(g_memcard_bios_events[MEMCARD_EVENT_HARDWARE_NEW_CARD]);
}

#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD)
/* Run _card_info on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times. Returns the last result (a rejected call counts
 * as a timeout; the opening's own body counts it as an error). */
memcard_result_e memcard_info_retry(s32 port, s32 retry_limit) {
    s32 i;
    memcard_result_e result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_info(port) == 1) {
            result = memcard_bios_event_wait();
            if (result == MEMCARD_RESULT_COMPLETE)
                break;
        } else {
            result = MEMCARD_RESULT_TIMEOUT;
        }
    }
    return result;
}

/* Run _card_load on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times two frames apart. Returns the last result (a
 * rejected call counts as a timeout). */
static memcard_result_e card_load_retry(s32 port, s32 retry_limit) {
    s32 i;
    memcard_result_e result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_load(port) == 1) {
            result = memcard_bios_event_wait();
            if (result == MEMCARD_RESULT_COMPLETE)
                break;
        } else {
            result = MEMCARD_RESULT_TIMEOUT;
        }
        VSync(2);
    }
    return result;
}

/* Clear the "new card" state of `port` with _card_clear, trying up to
 * `retry_limit` times. Returns MEMCARD_RESULT_COMPLETE once the hardware
 * event or a follow-up info request reports completion, otherwise the last
 * failure (a rejected call counts as a timeout). */
static memcard_result_e new_card_clear_retry(s32 port, s32 retry_limit) {
    s32 i;
    memcard_result_e result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_clear(port) == 1) {
            result = hardware_event_wait();
            if (result >= MEMCARD_RESULT_NEW_CARD)
                result = MEMCARD_RESULT_ERROR;
            if (result == MEMCARD_RESULT_COMPLETE)
                break;
            if (memcard_operation_wait(1) == MEMCARD_RESULT_COMPLETE) {
                result = MEMCARD_RESULT_COMPLETE;
                break;
            }
        } else {
            result = MEMCARD_RESULT_TIMEOUT;
        }
    }
    return result;
}

/* Poll the selected card slot: start a _card_info request when none is
 * pending and return its outcome (MEMCARD_RESULT_PENDING while it runs).
 * A set bit 0 of _card_status with no event yet counts as a timeout. A result
 * other than "complete" is checked again with a retried info request, and an
 * error is reported as a timeout. The opening's own body reissues the request
 * instead of timing out. */
memcard_result_e memcard_operation_poll(void) {
    s32 status;
    memcard_result_e result;

    if (g_memcard_info_pending == 0) {
        _card_info(g_memcard_slot << 4);
        g_memcard_info_pending = 1;
    }

    status = _card_status(g_memcard_slot) & 1;
    result = memcard_bios_event_poll();
    if (status != 0 && result == MEMCARD_RESULT_PENDING) {
        result = MEMCARD_RESULT_TIMEOUT;
    }
    if (result >= MEMCARD_RESULT_COMPLETE) {
        g_memcard_info_pending = 0;
    }
    if (result > MEMCARD_RESULT_COMPLETE) {
        result = memcard_info_retry(g_memcard_slot << 4, 2);
    }
    if (result == MEMCARD_RESULT_ERROR) {
        result = MEMCARD_RESULT_TIMEOUT;
    }
    return result;
}

/* Run the card status poll to a result, repeating it up to `retry_limit`
 * times until it reports completion. Returns the last result. */
memcard_result_e memcard_operation_wait(s32 retry_limit) {
    memcard_result_e result;
    s32 i = 0;

    while (i < retry_limit) {
        do {
            result = memcard_operation_poll();
        } while (result == MEMCARD_RESULT_PENDING);
        if (result == MEMCARD_RESULT_COMPLETE)
            break;
        i++;
    }
    return result;
}

/* Count the files of the selected slot into `entries`: the game's saves, or
 * every file when `all_files` is set. */
s32 memcard_saves_enumerate(DIRENTRY* entries, s32 all_files) {
    s32 count;
    char path[128];

    if (g_memcard_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    /* The game's own saves carry the Japanese product code, SLPS-00700, in this release too. */
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
s32 memcard_free_blocks_count(const DIRENTRY* entries, s32 entry_count) {
    s32 i;
    s32 used_blocks = 0;

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
 * card, then load its directory. The BIOS card calls take the port as slot << 4 (0x00 or 0x10).
 * Returns MEMCARD_RESULT_COMPLETE,
 * MEMCARD_RESULT_NEW_CARD (the load still reports a new, unformatted card)
 * or MEMCARD_RESULT_TIMEOUT for any failure. */
memcard_result_e memcard_slot_open(void) {
    memcard_result_e result = memcard_operation_wait(10);

    if (result == MEMCARD_RESULT_NEW_CARD) {
        result = new_card_clear_retry(g_memcard_slot << 4, 10);
        if (result != MEMCARD_RESULT_COMPLETE)
            return MEMCARD_RESULT_TIMEOUT;
    }
    if (result == MEMCARD_RESULT_COMPLETE)
        result = card_load_retry(g_memcard_slot << 4, 30);
    if (result == MEMCARD_RESULT_NEW_CARD)
        return MEMCARD_RESULT_NEW_CARD;
    if (result != MEMCARD_RESULT_COMPLETE)
        result = MEMCARD_RESULT_TIMEOUT;
    return result;
}

/* Format the selected memory card, calling format() three times, and return
 * the last call's result; 0 when the card is not ready. */
s32 memcard_slot_format(void) {
    s32 result;
    char path[8];

    result = memcard_operation_wait(10);
    if (result > MEMCARD_RESULT_COMPLETE)
        return 0;
    if (g_memcard_slot == 0)
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
 * g_file_descriptor), -1 when every open failed, or -2 when the
 * previous file would not close or the card did not become ready. */
s32 memcard_file_open_retry(const char* filename, s32 mode) {
    s32 i;
    s32 result;
    char path[128];

    i = 0;
    if (s_file_descriptor >= 0) {
        result = memcard_file_close_retry(s_file_descriptor);
        if (result == 0) {
            return -2;
        }
    }

    if (slot_get() == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, filename);

    while (i < 40) {
        result = memcard_operation_wait(10);
        if (result != MEMCARD_RESULT_COMPLETE) {
            return -2;
        }
        s_file_descriptor = open(path, mode);
        if (s_file_descriptor != -1) {
            break;
        }
        i++;
    }
    return s_file_descriptor;
}

/* Close a memory card file, trying up to ten times; each try first waits for the pending card
 * operation. Forget the open descriptor and return 1 only when the last wait completed and the
 * close succeeded. */
s32 memcard_file_close_retry(s32 descriptor) {
    s32 i;
    s32 wait_success;
    s32 close_result;

    for (i = 0; i < 10; i++) {
        wait_success = memcard_operation_wait(10) == MEMCARD_RESULT_COMPLETE;
        close_result = close(descriptor);
        if (close_result == descriptor) {
            break;
        } else {
            wait_success = 0;
        }
    }
    s_file_descriptor = -1;
    return wait_success;
}

/* lseek with up to ten tries, waiting for the card before each. Returns the
 * new position, or -1 when the card is not ready or every try fails. */
s32 memcard_file_seek_retry(s32 descriptor, s32 offset, s32 origin) {
    s32 i;
    /* The unoptimized local layout retains one unreferenced 32-bit value. */
    s32 unused_local;
    s32 result;

    for (i = 0; i < 10; i++) {
        if (memcard_operation_wait(10) != MEMCARD_RESULT_COMPLETE)
            return -1;
        result = lseek(descriptor, offset, origin);
        if (result != -1)
            break;
    }
    return result;
}

/* Write `size` bytes of `source` to `filename` on the selected slot, creating
 * the file first when `create_file` is set, in up to 40 write attempts. Returns the
 * bytes written, or -1 when an open, close or error check fails.
 *
 * The create mode is the BIOS create flag 0x200 with the file's block count in the high half;
 * mode 2 opens for writing. Each attempt seeks by the bytes written so far from the current
 * position (origin 1) rather than to them, so after a short write the next attempt would skip
 * ahead (retail; a whole save normally goes in one write). */
s32 save_file_write(const char* filename, const save_image_t* source, u32 size, s32 create_file) {
    s32 error;
    s32 result;
    s32 retries;
    s32 total_written;

    if (create_file != 0) {
        /* Taking the field address preserves the unoptimized creation-mode load. */
        result = memcard_file_open_retry(filename, (*(&source->header.block_count) << 16) | 0x200);
        if (result <= -1) {
            return -1;
        } else {
            if (memcard_file_close_retry(s_file_descriptor) == 0) {
                return -1;
            }
            result = memcard_file_open_retry(filename, 2);
            if (result <= -1) {
                return -1;
            }
        }
        create_file = 0;
    } else {
        result = memcard_file_open_retry(filename, 2);
        if (result < 0) {
            return -1;
        }
    }

    error = _get_error(s_file_descriptor);
    if (error != 0) {
        memcard_file_close_retry(s_file_descriptor);
        return -1;
    }

    total_written = 0;
    retries = 0;
    while (retries < 40) {
        if (memcard_file_seek_retry(s_file_descriptor, total_written, 1) < 0) {
            break;
        }

        result = write(s_file_descriptor, (const u8*)source + total_written, size - total_written);
        if (result != -1) {
            total_written += result;
            if (total_written >= size) {
                break;
            }
        }
        retries++;
    }

    if (memcard_file_close_retry(s_file_descriptor) == 0) {
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
s32 memcard_file_delete_retry(const char* filename) {
    char path[256];
    /* The unoptimized frame reserves one 32-bit value between the path and result. */
    s32 path_padding;
    s32 result;
    s32 i;

    if (g_memcard_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, filename);

    for (i = 0; i < 10; i++) {
        if (memcard_operation_wait(10) != MEMCARD_RESULT_COMPLETE) {
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
 * returns the bytes read. The seek has the same relative-offset quirk as
 * save_file_write. */
s32 save_file_read(const char* filename, u8* destination, u32 size) {
    u32 total;
    s32 result = memcard_file_open_retry(filename, 1);
    s32 i;

    if (result < 0) {
        return -1;
    }
    if (_get_error(s_file_descriptor) != 0) {
        memcard_file_close_retry(s_file_descriptor);
        return -1;
    }
    total = 0;
    for (i = 0; i < 40; i++) {
        if (memcard_file_seek_retry(s_file_descriptor, total, 1) < 0) {
            break;
        }
        result = read(s_file_descriptor, destination + total, size - total);
        if (result != -1) {
            total += result;
            if (total >= size) {
                break;
            }
        }
    }
    if (memcard_file_close_retry(s_file_descriptor) == 0) {
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
s32 memcard_file_read_retry(s32 descriptor, void* destination, s32 size) {
    s32 error;
    s32 result;
    s32 i;
    s32 initial_position = memcard_file_seek_retry(descriptor, 0, 1);

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
        if (memcard_file_seek_retry(descriptor, initial_position, 0) < 0) {
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
s32 memcard_file_write_retry(s32 descriptor, const void* source, s32 size) {
    s32 error;
    s32 result;
    s32 i;
    s32 initial_position = memcard_file_seek_retry(descriptor, 0, 1);

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
        if (memcard_file_seek_retry(descriptor, initial_position, 0) < 0) {
            break;
        }
    }
    if (result != size) {
        result = -1;
    }
    return result;
}
#endif
