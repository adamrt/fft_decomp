#include "fft/world.h"
#include "psx/types.h"

/* Menu activity summary: 2 while the menu-building thread runs, else 4 when
 * any of thread slots 4..8 runs, else 0. */
s32 menu_get_thread_activity_variant(void) {
    s32 i;

    if (thread_is_running_asm_variant(THREAD_SLOT_MENU_BUILDING) != 0) {
        return 2;
    }
    for (i = 4; i < 9; i++) {
        if (thread_is_running_asm_variant(i) != 0) {
            break;
        }
    }
    return (i != 9) << 2;
}
