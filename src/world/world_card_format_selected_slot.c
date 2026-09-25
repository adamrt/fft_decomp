#include "fft/world.h"
#include "psx/libc.h"
#include "psx/types.h"

extern s32 FileFormat(char* name);

/* Format the selected card slot three times once it reports ready.
 *
 * The device name is a strcpy of the "bu00:" /
 * "bu10:" literals shared with the other card routines at 0x800e0200; GCC
 * expands the constant-source strcpy into the 6-byte lw/lh block move. */
s32 world_card_format_selected_slot(void) {
    char name[8];

    if (world_card_wait_for_selected_status(10) > CARD_IO_RESULT_COMPLETE) {
        return 0;
    }
    if (g_world_card_selected_slot == 0) {
        strcpy(name, "bu00:");
    } else {
        strcpy(name, "bu10:");
    }
    FileFormat(name);
    FileFormat(name);
    return FileFormat(name);
}
