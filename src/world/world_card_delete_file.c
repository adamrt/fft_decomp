#include "fft/card.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

extern s32 FileDelete(char* name);

/* Deletes `name` from the selected memory card, retrying FileDelete up to
 * ten times; gives up with 0 when world_card_wait_for_selected_status reports a problem. */
s32 world_card_delete_file(char* name) {
    char path[256];
    s32 i;
    s32 result;

    if (g_world_card_selected_slot == 0) {
        strcpy(path, "bu00:");
    } else {
        strcpy(path, "bu10:");
    }
    strcat(path, name);
    for (i = 0; i < 10; i++) {
        if (world_card_wait_for_selected_status(10) != CARD_IO_RESULT_COMPLETE) {
            return 0;
        }
        result = FileDelete(path);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}
