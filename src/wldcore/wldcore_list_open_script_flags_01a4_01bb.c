#include "fft/wldcore.h"

/* Opens a panel list of the set script variables 0x1a4-0x1bb (entry values
 * 0x9800 + index), shows up to eight rows at (0x58, 0x50) with width 0x8c,
 * hides the parent list window's content render record and appends its
 * 0x5c-byte record tagged 0x2e. Twin of 0x80087b94. */
void wldcore_list_open_script_flags_01a4_01bb(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < 24; i++) {
        if (world_script_get_variable(i + 0x1A4) != 0) {
            *values = i - 0x6800;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    dimensions.x = 0x8C;
    origin.x = 0x58;
    dimensions.y = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(0x14, count, i);
    wldcore_window_start_panel_render_thread(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    g_main_system_flags |= 0x800;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.content_render]
        .flags |= 0x10;
    world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_SCRIPT_FLAGS_01A4_01BB;
    g_wldcore_menu_stack_depth = record_index + 1;
}
