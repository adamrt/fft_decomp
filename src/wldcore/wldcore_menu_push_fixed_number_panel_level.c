#include "fft/wldcore.h"

/* Queue the fixed "9999"-style number panel used on the world map and append
 * its 0x5c-byte record. Sibling of wldcore_menu_push_number_panel_level_with_argument. */
void wldcore_menu_push_fixed_number_panel_level(void) {
    wldcore_point32_t origin;
    s32 record_index;

    g_wldcore_menu_result = 0x60;
    g_wldcore_menu_ordering_table_offset = 3;
    origin.x = -0x28;
    origin.y = -0x16;
    g_wldcore_active_menu_value = 0;
    wldcore_window_init_number_panel_render_thread(0xC, 0x3E70000, 0xB826, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_FIXED_NUMBER_PANEL;
    g_wldcore_menu_stack_depth = record_index + 1;
}
