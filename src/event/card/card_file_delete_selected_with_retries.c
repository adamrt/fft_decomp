#include "fft/event_card.h"
#include "psx/libapi.h"
#include "psx/types.h"

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
