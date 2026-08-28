#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Opens a panel list of the set script flags 0x1bc-0x1fb (entry values
 * 0xe800 + flag index), shows up to eight rows at (0x4c, 0x4f) with width
 * 0x94, and appends its 0x5c-byte record tagged 0x1d. Twin of
 * wldcore_list_open_treasures, but walks the output arrays by pointer. */
void wldcore_list_open_story_events(void) {
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
    for (; i < 64; i++) {
        if (world_script_get_variable(i + 0x1BC) != 0) {
            *values = i - 0x1800;
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
    dimensions.x = 0x94;
    origin.x = 0x4C;
    dimensions.y = i;
    origin.y = 0x4F;
    wldcore_list_clamp_cursor_state(0xA, count, i);
    wldcore_window_start_panel_render_thread(
        0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_ordering_table_offset = 9;
    record_index = g_wldcore_menu_stack_depth;
    g_main_system_flags |= 0x800;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_STORY_EVENTS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
