#include "fft/world.h"
#include "psx/libapi.h"
#include "psx/types.h"

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
        result = world_card_wait_for_selected_status(10);
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
