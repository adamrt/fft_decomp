#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens the active-proposition panel list as menu level type 0x12.
 *
 * Complete propositions are skipped. Each surviving proposition contributes
 * its display value, row flags, and source-record index to the new menu level.
 * Before pushing the level, this hides the previous window pair when visible.
 * The otherwise unused point record preserves the target's 24-byte local area;
 * GCC removes its operations but retains its stack slot. */
void wldcore_list_open_propositions(void) {
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 loaded_count;
    s32 previous;
    s32 total;
    s32 entry_value_base;
    s16* row_flags;
    s32 active_row_flag;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_IN_PROGRESS, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        row_flags = g_wldcore_list_row_flags;
        active_row_flag = 8;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED) == 0) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RECALLED) != 0) {
                    row_flags[count] = active_row_flag;
                } else {
                    row_flags[count] = 0;
                }
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 6) {
        i = count;
    } else {
        i = 6;
    }
    layout.text_width = 0x96;
    origin.x = 0x4C;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS, g_main_save_proposition_count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    previous = g_wldcore_menu_stack_depth - 1;
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    if ((g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
                .flags
            & WORLD_RENDER_RECORD_HIDDEN)
        != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITIONS;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
