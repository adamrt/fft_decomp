#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Opens a panel list of the set script variables 0x3c0-0x3ff (entry values
 * 0xf000 + index * 16 + variable value), shows up to eight rows at
 * (0x80, 0x50) with width 0x60, appends its 0x5c-byte record tagged 0x1f and
 * hides the parent list window's content render record. Twin of 0x80087b94. */
void wldcore_list_open_set_script_variables_03c0_03ff(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 value;
    s32 record_index;

    world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < 64; i++) {
        value = world_script_get_variable(i + 0x3C0);
        if (value != 0) {
            *values = (i << 4) + (s16)(value - 0x1000);
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
    dimensions.x = 0x60;
    origin.x = 0x80;
    dimensions.y = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(0xB, count, i);
    wldcore_window_start_panel_render_thread(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_ordering_table_offset = 9;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= 0x800;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index - 1].list_window.content_render]
        .flags |= 0x10;
    g_wldcore_menu_stack_types_next[record_index] = WLDCORE_MENU_LEVEL_SET_SCRIPT_VARIABLES_03C0_03FF;
    g_wldcore_menu_stack_depth = record_index + 1;
}
