#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/wldcore.h"

/* Opens the active-proposition panel list as menu level type 0x12.
 *
 * Complete propositions are skipped. Each surviving proposition contributes
 * its display value, row flags, and source-record index to the new menu level.
 * Before pushing the level, this hides the previous window pair when visible.
 * The otherwise unused point record preserves the target's 24-byte local area;
 * GCC removes its operations but retains its stack slot. */
void wldcore_list_open_propositions(void) {
    wldcore_point32_t origin;
    wldcore_point32_t dimensions;
    wldcore_point32_t unused_point;
    s32 count;
    s32 i;
    s32 loaded_count;
    s32 previous;
    s32 total;
    s32 entry_value_base;
    s16* row_flags;
    s32 active_row_flag;

    world_thread_set_parameters(0xE, 0x19, 0xB81B, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        entry_value_base = 0xD000;
        row_flags = g_wldcore_list_row_flags;
        active_row_flag = 8;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & 4) == 0) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                if ((g_main_active_propositions[i].flags & 2) != 0) {
                    row_flags[count] = active_row_flag;
                } else {
                    row_flags[count] = 0;
                }
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count = count;
    if (count < 6) {
        i = count;
    } else {
        i = 6;
    }
    dimensions.x = 0x96;
    origin.x = 0x4C;
    dimensions.y = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(5, g_main_save_proposition_count, i);
    wldcore_window_start_panel_render_thread(0xC,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count, &dimensions,
        &origin);
    previous = g_wldcore_menu_stack_depth - 1;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_result = 0x160;
    if ((g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
                .flags
            & 0x10)
        != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= 0x100;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITIONS;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
