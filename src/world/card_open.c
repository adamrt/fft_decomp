/* Formatting the selected slot and opening a file with retries. */

#include "fft/world.h"
#include "psx/libapi.h"
#include "psx/types.h"

/* Format the selected card slot three times once it reports ready.
 *
 * The device name is a strcpy of the "bu00:" /
 * "bu10:" literals shared with the other card routines at 0x800e0200; GCC
 * expands the constant-source strcpy into the 6-byte lw/lh block move. */
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
