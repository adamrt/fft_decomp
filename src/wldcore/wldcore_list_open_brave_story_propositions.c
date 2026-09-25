#include "fft/wldcore.h"

/* Opens a panel list of the script variables 0x360-0x3bf with bit 0x04 set
 * (entry values 0x6000 + index), shows up to eight rows at (0x4c, 0x50) with
 * width 0x96, appends its 0x5c-byte record tagged 0x29 and hides the parent
 * list window's content render record. Twin of wldcore_list_open_story_events. */
void wldcore_list_open_brave_story_propositions(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s16* values;
    s16* flags;
    s32 count;
    s32 i;
    s32 record_index;

    world_thread_set_parameters(0xE, 0x19, 0xB848, 0);
    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    for (; i < 96; i++) {
        if (world_script_get_variable(i + 0x360) & 4) {
            *values = i + 0x6000;
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
    dimensions.x = 0x96;
    origin.x = 0x4C;
    dimensions.y = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(0x10, count, i);
    wldcore_window_start_panel_render_thread(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_ordering_table_offset = 8;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= 0x800;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index - 1].list_window.content_render]
        .flags |= 0x10;
    g_wldcore_menu_stack_types_next[record_index] = WLDCORE_MENU_LEVEL_BRAVE_STORY_PROPOSITIONS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
