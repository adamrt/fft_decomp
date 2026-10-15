#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens an eight-row panel list (entry values 0xc010 + row) whose rows are
 * flagged 8 when bit 32 + row of 0x8004c6bc is set, shown at (0x54, 0x50)
 * with width 0x8c; appends its record tagged 0x2d. Sibling of
 * wldcore_list_open_formation_tutorials. */
void wldcore_list_open_tutorial_mask_1_entries(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 i;
    s32 count;
    s32 record_index;

    for (i = 0; i < 8; i++) {
        g_wldcore_list_entry_values[i] = i - 0x3FF0;
        if (wldcore_test_bit(&g_main_tutorial_progress_masks[1], i) != 0) {
            g_wldcore_list_row_flags[i] = 8;
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
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_TUTORIAL_MASK_1_ENTRIES, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_MASK_1_ENTRIES;
    g_wldcore_menu_stack_depth = record_index + 1;
}
