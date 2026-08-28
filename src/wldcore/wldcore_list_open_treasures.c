#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Opens a panel list of the set script flags 0x321-0x34f (entry values
 * 0xe000 + flag index), shows up to eight rows at (0x88, 0x50) with width
 * 0x5a, and appends its 0x5c-byte record tagged 0x1a. Twin of 0x8007dfcc. */
void wldcore_list_open_treasures(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s32 count;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    count = 0;
    for (i = 0; i < 47; i++) {
        if (world_script_get_variable(i + 0x321) != 0) {
            g_wldcore_list_entry_values[count] = i - 0x2000;
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
    dimensions.x = 0x5A;
    origin.x = 0x88;
    dimensions.y = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(8, count, i);
    wldcore_window_start_panel_render_thread(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TREASURES;
    g_wldcore_menu_stack_depth = record_index + 1;
}
