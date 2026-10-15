#include "fft/wldcore.h"

/* Pop the propositions list level: toggle the input-blocked system flag and
 * unblock the menu buttons. */
/* Target 0x80072ef4. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_propositions_close_level(wldcore_menu_panel_level_t* level) {
    g_wldcore_menu_ordering_table_offset = 1;
    g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    --g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = 0;
}
