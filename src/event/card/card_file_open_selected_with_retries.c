#include "fft/event_card.h"
#include "psx/types.h"

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
