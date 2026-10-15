/* Save files. A small object between boundaries the binary proves. */

#include "fft/event_card.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* List the selected card slot's directory into `entries` and return the
 * count: the game's save files, or every file when `all_files` is set. */
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
