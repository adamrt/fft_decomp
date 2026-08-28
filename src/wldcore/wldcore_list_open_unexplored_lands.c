#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Opens a panel list of the set script flags 0x350-0x35f (entry values
 * 0xd800 + flag index), shows up to eight rows at (0x80, 0x50) with width
 * 0x60, and appends its 0x5c-byte record tagged 0x18. */
void wldcore_list_open_unexplored_lands(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s32 count;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    count = 0;
    for (i = 0; i < 16; i++) {
        if (world_script_get_variable(i + 0x350) != 0) {
            g_wldcore_list_entry_values[count] = i - 0x2800;
            g_wldcore_list_row_flags[count] = 0;
            count++;
        }
    }
    world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    dimensions.x = 0x60;
    origin.x = 0x80;
    dimensions.y = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(7, count, i);
    wldcore_window_start_panel_render_thread(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_UNEXPLORED_LANDS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
