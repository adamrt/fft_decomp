/* Save file and free block counts and the selected card probe. */

#include "fft/world.h"
#include "psx/types.h"

/* Counts the save files on the selected memory card into `dir`: every
 * FFT save when `all` is nonzero, otherwise only the BISLPS-00700FF7*
 * entries.  strcpy is left to the compiler builtin so the prefix copy is
 * expanded inline as the target shows. */
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
