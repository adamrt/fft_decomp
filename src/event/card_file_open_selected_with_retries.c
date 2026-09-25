#include "fft/event_card.h"
#include "psx/types.h"

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
        g_card_file_open_descriptor = FileOpen(path, mode);
        if (g_card_file_open_descriptor != -1) {
            break;
        }
        i++;
    }
    return g_card_file_open_descriptor;
}
