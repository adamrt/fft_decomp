#include "fft/card.h"
#include "psx/api.h"
#include "psx/types.h"

s32 card_file_delete_selected_with_retries(const char* filename) {
    char path[256];
    /* The retail O0 frame reserves one word between the path and result. */
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
        result = FileDelete(path);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}
