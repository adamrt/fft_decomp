#include "fft/wldcore.h"
#include "psx/types.h"

/* Target 0x80072404. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_rumors_close_level(wldcore_menu_panel_level_t* level) {
    g_wldcore_menu_ordering_table_offset = 1;
    g_main_system_flags ^= 4;
    --g_wldcore_menu_stack_depth;
    g_wldcore_menu_result = 0;
}
