#include "fft/battle_menu.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Report whether the menu layer is busy.
 *
 * Returns g_menu_transition_state when it is nonzero (4 during the unit
 * list fade-out), 2 while the menu-building thread runs or while menu input is
 * blocked, 4 when one of threads 4-9 is running, and 0 when all are idle. The `i
 * != 3` test inside the scan of threads 4-9 always passes. */
s32 battle_menu_is_still_building(void) {
    s32 i;
    s32 result;

    if (g_menu_transition_state != 0) {
        return g_menu_transition_state;
    }
    if (thread_is_running_asm_variant(THREAD_SLOT_MENU_BUILDING) != 0) {
        return 2;
    }
    i = 4;
    do {
        if (i != 3) {
            if (thread_is_running_asm_variant(i) != 0) {
                break;
            }
        }
        i += 1;
    } while (i < 0xA);
    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        result = i != 0xA;
        return result * 4;
    }
    return 2;
}
