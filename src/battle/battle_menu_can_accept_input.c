#include "fft/battle.h"

/* volatile view: the target reloads this global at every use. */
extern volatile u16 g_battle_menu_input_block_frames;

/* Returns 1 when a menu thread may act on input: no previous event thread
 * running, no third thread parameter, not in event mode, no help menu open,
 * and g_battle_menu_input_block_frames clear. */
s32 battle_menu_can_accept_input(void) {
    if (battle_thread_is_previous_running() != 0 || battle_thread_get_current_parameter_3() != 0 || g_event_mode != 0
        || g_battle_menu_help_open != 0 || g_battle_menu_input_block_frames != 0) {
        return 0;
    }
    return 1;
}
