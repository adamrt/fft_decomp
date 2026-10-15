#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens a seven-row panel list (entry values 0xc000 + row) whose rows are
 * flagged 8 when the matching bit of 0x8004c6bc is set, shown at
 * (0x54, 0x50) with width 0x8c; appends its record tagged 0x2c.
 *
 * The flag value lives in its own variable so the target keeps 8 in a saved
 * register across the loop. */
void wldcore_list_open_formation_tutorials(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 i;
    s32 count;
    s32 record_index;
    s32 enabled;

    i = 0;
    enabled = 8;
    for (; i < 7; i++) {
        g_wldcore_list_entry_values[i] = i + (s16)TEXT_ID_TUTORIAL_NAME_BASE;
        if (wldcore_test_bit(g_main_tutorial_progress_masks, i) != 0) {
            g_wldcore_list_row_flags[i] = enabled;
        } else {
            g_wldcore_list_row_flags[i] = 0;
        }
    }
    count = i;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = i;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x8C;
    origin.x = 0x54;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_FORMATION_TUTORIALS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_FORMATION_TUTORIALS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
