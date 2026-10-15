/* The memory card library's slot selection, BIOS and hardware event polling, info, load and create with retries,
 * operation result polling, save file enumeration and free block counts, and the open-or-create probe, with each
 * program's own bodies where they diverged. */

#include "fft/event_card.h"
#include "fft/main.h"
#include "fft/open.h"
#include "fft/world.h"
#include "psx/libapi.h"
#include "psx/libetc.h"
#include "psx/types.h"

#if defined(FFT_WORLD) || defined(FFT_OPENING)
void card_set_slot(s32 slot) {
    g_card_slot = slot;
}
#endif

#ifdef FFT_WORLD
s32 world_card_get_selected_slot(void) {
    return g_card_slot;
}
#endif

#ifdef FFT_WORLD
/* Empty. Called before a soft reset and when the memory card screen closes. */
void world_card_close_stub(void) {
}
#endif

#ifdef FFT_EVENT_CARD
void card_io_set_selected_slot(s32 value) {
    g_card_io_selected_slot = value;
}
#endif

#ifdef FFT_EVENT_CARD
s32 card_io_get_selected_slot(void) {
    return g_card_io_selected_slot;
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

#if defined(FFT_WORLD) || defined(FFT_OPENING)
/* Block until a BIOS card event fires and return it. Returns timeout when card
 * status bit 0 of the selected slot is set and no event has fired. */
s32 card_wait_for_bios_event_variant(void) {
    s32 card_ready;
    s32 event_result;

    do {
        card_ready = _card_status(g_card_slot) & 1;
        event_result = card_io_poll_bios_event_result();
        if (event_result >= CARD_IO_RESULT_COMPLETE) {
            return event_result;
        }
    } while (card_ready == 0);

    return CARD_IO_RESULT_TIMEOUT;
}
#endif

#ifdef FFT_EVENT_CARD
/* Poll the BIOS card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * CARD_IO_RESULT_TIMEOUT. */
s32 card_wait_for_bios_event_variant(void) {
    s32 result;
    s32 ready;

    while (1) {
        ready = _card_status(g_card_io_selected_slot) & 1;
        result = card_io_poll_bios_event_result();
        if (result >= CARD_IO_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return CARD_IO_RESULT_TIMEOUT;
    }
}
#endif

void card_io_consume_bios_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_BIOS_NEW_CARD]);
}

/* Test the four hardware card events (done, error, timeout, new card) and return
 * the matching CARD_IO_RESULT_*, or CARD_IO_RESULT_PENDING when none has
 * fired. A result clears all four events. */
s32 card_io_poll_hardware_event_result(void) {
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

#if defined(FFT_WORLD) || defined(FFT_OPENING)
/* Block until a card hardware event fires and return it. Returns timeout when
 * card status bit 0 of the selected slot is set and no event has fired. Nothing
 * in OPEN.BIN calls it. */
s32 card_wait_for_hardware_event_variant(void) {
    s32 card_ready;
    s32 event_result;

    do {
        card_ready = _card_status(g_card_slot) & 1;
        event_result = card_io_poll_hardware_event_result();
        if (event_result >= CARD_IO_RESULT_COMPLETE) {
            return event_result;
        }
    } while (card_ready == 0);

    return CARD_IO_RESULT_TIMEOUT;
}
#endif

#ifdef FFT_EVENT_CARD
/* Poll the hardware card event until it reports a result. A poll that finds
 * nothing while bit 0 of _card_status is set ends the wait with
 * CARD_IO_RESULT_TIMEOUT. */
s32 card_wait_for_hardware_event_variant(void) {
    s32 result;
    s32 ready;

    while (1) {
        ready = _card_status(g_card_io_selected_slot) & 1;
        result = card_io_poll_hardware_event_result();
        if (result >= CARD_IO_RESULT_COMPLETE)
            return result;
        if (ready != 0)
            return CARD_IO_RESULT_TIMEOUT;
    }
}
#endif

/* Consume the four hardware-card event notifications in handle order. */
void card_io_consume_hardware_events(void) {
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_DONE]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_ERROR]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_TIMEOUT]);
    TestEvent(g_main_card_bios_events[MAIN_CARD_EVENT_HARDWARE_NEW_CARD]);
}

#ifdef FFT_WORLD
s32 card_get_info_with_retries_variant(s32 channel, s32 count) {
    s32 result;
    s32 i;

    for (i = 0; i < count; i++) {
        if (_card_info(channel) == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE) {
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
#endif

#ifdef FFT_WORLD
/* Retries _card_load for `attempts` frames; returns the BIOS event result or
 * timeout when _card_load does not start. */
s32 world_card_load_with_retries(s32 channel, s32 attempts) {
    s32 i;
    s32 result;

    for (i = 0; i < attempts; i++) {
        s32 status = _card_load(channel);

        result = CARD_IO_RESULT_TIMEOUT;
        if (status == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE) {
                break;
            }
        }
        VSync(2);
    }
    return result;
}
#endif

#ifdef FFT_EVENT_CARD
/* Run _card_info on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times. Returns the last result (a rejected call counts
 * as a timeout). */
s32 card_io_get_info_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_info(port) == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
#endif

#ifdef FFT_EVENT_CARD
/* Run _card_load on `port` until the BIOS event reports completion, trying
 * up to `retry_limit` times two frames apart. Returns the last result (a
 * rejected call counts as a timeout). */
s32 card_io_load_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_load(port) == 1) {
            result = card_wait_for_bios_event_variant();
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
        VSync(2);
    }
    return result;
}
#endif

#if defined(FFT_WORLD) || defined(FFT_EVENT_CARD)
/* Clear the "new card" state of `port` with _card_clear, trying up to
 * `retry_limit` times. Returns CARD_IO_RESULT_COMPLETE once the hardware
 * event or a follow-up info request reports completion, otherwise the last
 * failure (a rejected call counts as a timeout). */
s32 card_io_create_new_with_retries(s32 port, s32 retry_limit) {
    s32 i;
    s32 result;

    for (i = 0; i < retry_limit; i++) {
        if (_card_clear(port) == 1) {
            result = card_wait_for_hardware_event_variant();
            if (result >= CARD_IO_RESULT_NEW_CARD)
                result = CARD_IO_RESULT_ERROR;
            if (result == CARD_IO_RESULT_COMPLETE)
                break;
            if (card_io_wait_operation_result_variant(1) == CARD_IO_RESULT_COMPLETE) {
                result = CARD_IO_RESULT_COMPLETE;
                break;
            }
        } else {
            result = CARD_IO_RESULT_TIMEOUT;
        }
    }
    return result;
}
#endif

#ifdef FFT_WORLD
s32 card_poll_info_result_variant(void) {
    s32 busy;
    s32 result;

    if (g_card_info_pending == 0) {
        _card_info(g_card_slot * 0x10);
        g_card_info_pending = 1;
    }
    busy = _card_status(g_card_slot) & 1;
    result = card_io_poll_bios_event_result();
    if (busy != 0 && result == CARD_IO_RESULT_PENDING) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_card_info_pending = 0;
        if (result > CARD_IO_RESULT_COMPLETE) {
            result = card_get_info_with_retries_variant(g_card_slot * 0x10, 2);
        }
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
#endif

#ifdef FFT_EVENT_CARD
/*
 * Poll the selected card slot: start a _card_info request when none is
 * pending and return its outcome (CARD_IO_RESULT_PENDING while it runs).
 *
 * A set bit 0 of _card_status with no event yet counts as a timeout. A result other than
 * "complete" is checked again with a retried info request, and an error is
 * reported as a timeout.
 */
s32 card_io_poll_operation_result(void) {
    s32 status;
    s32 result;

    if (g_card_io_operation_pending == 0) {
        _card_info(g_card_io_selected_slot << 4);
        g_card_io_operation_pending = 1;
    }

    status = _card_status(g_card_io_selected_slot) & 1;
    result = card_io_poll_bios_event_result();
    if (status != 0 && result == CARD_IO_RESULT_PENDING) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    if (result >= CARD_IO_RESULT_COMPLETE) {
        g_card_io_operation_pending = 0;
    }
    if (result > CARD_IO_RESULT_COMPLETE) {
        result = card_io_get_info_with_retries(g_card_io_selected_slot << 4, 2);
    }
    if (result == CARD_IO_RESULT_ERROR) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
#endif

#ifdef FFT_WORLD
/* Poll the selected memory card until it reports a settled state.
 *
 * Each attempt retries while the result is pending. Returns the first
 * non-complete status, or the last status once `attempts` settled polls have
 * completed. */
s32 card_io_wait_operation_result_variant(s32 attempts) {
    s32 i;
    s32 status;

    for (i = 0; i < attempts; i++) {
        do {
            status = card_poll_info_result_variant();
        } while (status == CARD_IO_RESULT_PENDING);
        if (status == CARD_IO_RESULT_COMPLETE) {
            break;
        }
    }
    return status;
}
#endif

#ifdef FFT_EVENT_CARD
/* Run the card status poll to a result, repeating it up to `retry_limit`
 * times until it reports completion. Returns the last result. */
s32 card_io_wait_operation_result_variant(s32 retry_limit) {
    s32 result;
    s32 i = 0;

    while (i < retry_limit) {
        do {
            result = card_io_poll_operation_result();
        } while (result == CARD_IO_RESULT_PENDING);
        if (result == CARD_IO_RESULT_COMPLETE)
            break;
        i++;
    }
    return result;
}
#endif

#ifdef FFT_WORLD
s32 world_card_count_save_files(DIRENTRY* dir, s32 all) {
    char path[128];
    s32 count;

    if (g_card_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    if (all == 0) {
        strcat(path, "BISLPS-00700FF7*");
    } else {
        strcat(path, "*");
    }
    count = 0;
    if (firstfile(path, dir) == dir) {
        do {
            count++;
            dir++;
        } while (nextfile(dir) == dir);
    }
    return count;
}
#endif

#ifdef FFT_WORLD
/*
 * Count the free blocks after rounding each directory entry up to 8 KiB.
 *
 * The card has 15 usable blocks. Indexed traversal and the early full-card
 * return preserve the target's loop scheduling and common return.
 */
s32 world_card_count_free_blocks(const DIRENTRY* entries, s32 count) {
    s32 used = 0;
    s32 i;

    for (i = 0; i < count; i++) {
        used += entries[i].size >> 13;
        used += (entries[i].size & 0x1fff) != 0;
    }
    if (used >= 16) {
        return 0;
    }
    return 15 - used;
}
#endif

#ifdef FFT_WORLD
/* Check the selected memory card and normalize failures to timeout. */
s32 world_card_probe_selected(void) {
    s32 result = card_io_wait_operation_result_variant(10);

    if (result == CARD_IO_RESULT_NEW_CARD) {
        if (card_io_create_new_with_retries(g_card_slot * 0x10, 10) != CARD_IO_RESULT_COMPLETE) {
            return CARD_IO_RESULT_TIMEOUT;
        }
        result = world_card_load_with_retries(g_card_slot * 0x10, 0x1e);
    } else if (result == CARD_IO_RESULT_COMPLETE) {
        result = world_card_load_with_retries(g_card_slot * 0x10, 0x1e);
    }
    if (result == CARD_IO_RESULT_NEW_CARD) {
        return CARD_IO_RESULT_NEW_CARD;
    }
    if (result != CARD_IO_RESULT_COMPLETE) {
        result = CARD_IO_RESULT_TIMEOUT;
    }
    return result;
}
#endif

#ifdef FFT_EVENT_CARD
s32 card_file_enumerate_saves(DIRENTRY* entries, s32 all_files) {
    s32 count;
    char path[128];

    if (g_card_io_selected_slot == 0) {
        strcpy(path, g_card_file_slot_0_path);
    } else {
        strcpy(path, g_card_file_slot_1_path);
    }
    if (all_files == 0) {
        strcat(path, g_card_save_file_pattern);
    } else {
        strcat(path, g_card_file_all_pattern);
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
#endif

#ifdef FFT_EVENT_CARD
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
#endif

#ifdef FFT_EVENT_CARD
/* Bring the selected card to a readable state: acknowledge a newly inserted
 * card, then load its directory. Returns CARD_IO_RESULT_COMPLETE,
 * CARD_IO_RESULT_NEW_CARD (the load still reports a new, unformatted card)
 * or CARD_IO_RESULT_TIMEOUT for any failure. */
s32 card_io_open_or_create_selected_card(void) {
    s32 result = card_io_wait_operation_result_variant(10);

    if (result == CARD_IO_RESULT_NEW_CARD) {
        result = card_io_create_new_with_retries(g_card_io_selected_slot << 4, 10);
        if (result != CARD_IO_RESULT_COMPLETE)
            return CARD_IO_RESULT_TIMEOUT;
    }
    if (result == CARD_IO_RESULT_COMPLETE)
        result = card_io_load_with_retries(g_card_io_selected_slot << 4, 30);
    if (result == CARD_IO_RESULT_NEW_CARD)
        return CARD_IO_RESULT_NEW_CARD;
    if (result != CARD_IO_RESULT_COMPLETE)
        result = CARD_IO_RESULT_TIMEOUT;
    return result;
}
#endif
