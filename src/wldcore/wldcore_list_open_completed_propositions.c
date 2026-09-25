#include "fft/wldcore.h"

/* Opens the list of active propositions at the current location as menu
 * level type 0xe.
 *
 * Restores the parent level's hidden window pair first. Starts WORLD thread
 * 14 on 0x8010f250 if needed with text 0xb819, then lists each proposition
 * whose flag 4 is set and whose location is g_wldcore_map_projection_state.marker.kind:
 * display value 0xd000 + proposition_id, clear row flags, source index in
 * entries. With no entries it rebuilds the parent window pair through
 * wldcore_bar_build_menu_rows and resumes the parent handler instead of pushing.
 * Twin of wldcore_list_open_propositions.
 *
 * The location is read through a pointer local: that keeps the load inside
 * the loop (the array stores may alias it) with only its address hoisted,
 * as in the target. The unused point record preserves the 24-byte local
 * area. */
void wldcore_list_open_completed_propositions(void) {
    wldcore_point32_t origin;
    wldcore_point32_t dimensions;
    wldcore_point32_t unused_point;
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
        & 0x10) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= 0x100;
    }
    if (world_thread_is_running(0xE) == 0) {
        world_thread_start(0xE, world_text_message_box_thread);
    }
    world_thread_set_parameters(0xE, 0x19, 0xB819, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        location = &g_wldcore_map_projection_state.marker.kind;
        entry_value_base = 0xD000;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & 4) && g_main_active_propositions[i].location == *location) {
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
        dimensions.x = 0x96;
        origin.x = 0x48;
        dimensions.y = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(4, count, i);
        wldcore_window_start_panel_render_thread(0xC,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_list.entry_count, &dimensions,
            &origin);
    } else {
        wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COMPLETED_PROPOSITIONS;
    g_wldcore_menu_stack_depth++;
}
