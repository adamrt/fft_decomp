#include "fft/battle_event.h"
#include "fft/battle_menu.h"
#include "fft/thread.h"

/* Returns 1 when a menu thread may act on input: no previous event thread
 * running, no third thread parameter, not in event mode, no help menu open,
 * and g_battle_menu_input_block_frames clear. */
s32 menu_can_accept_input(void) {
    if (battle_thread_is_previous_running() != 0 || battle_thread_get_current_parameter_3() != 0
        || g_menu_input_lock_mode != 0 || g_menu_help_open != 0 || g_battle_menu_input_block_frames != 0) {
        return 0;
    }
    return 1;
}
