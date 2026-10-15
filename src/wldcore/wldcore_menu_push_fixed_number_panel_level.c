#include "fft/wldcore.h"
#include "psx/pad.h"

/* Queue the fixed "9999"-style number panel used on the world map and append
 * its 0x5c-byte record. Sibling of wldcore_menu_push_number_panel_level_with_argument. */
void wldcore_menu_push_fixed_number_panel_level(void) {
    point32_t origin;
    s32 record_index;

    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS;
    g_wldcore_menu_ordering_table_offset = 3;
    origin.x = -0x28;
    origin.y = -0x16;
    g_wldcore_active_menu_value = 0;
    wldcore_window_init_number_panel_render_thread(
        WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0x3E70000, WLDCORE_TEXT_ID_PARTY_NUMBER_LABEL, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_FIXED_NUMBER_PANEL;
    g_wldcore_menu_stack_depth = record_index + 1;
}
