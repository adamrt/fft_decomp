/* Card info polling, operation result waits, save file and free block counts and the selected card probe. */

#include "fft/main.h"
#include "fft/world.h"
#include "psx/types.h"

/* Issue _card_info on the selected slot once, then poll for its result. */
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
