#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

void wldcore_list_open_completed_propositions(void) {
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 total;
    s32 previous;
    s32 loaded_count;
    s32* location;
    s32 entry_value_base;

    previous = g_wldcore_menu_stack_depth - 1;
    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags
        & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_RETURNED, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        location = &g_wldcore_map_projection_state.marker.kind;
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED)
                && g_main_active_propositions[i].location == *location) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                g_wldcore_list_row_flags[count] = 0;
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }
    if (count != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        layout.text_width = 0x96;
        origin.x = 0x48;
        layout.visible_rows = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_COMPLETED_PROPOSITIONS, count, i);
        wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count, &layout,
            &origin);
    } else {
        wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS;
    g_wldcore_menu_stack_depth++;
}
