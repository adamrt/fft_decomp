#include "fft/wldcore.h"

/* Opens the town menu's proposition-location list (row kind 1 of
 * wldcore_bar_handle_menu_input). If the parent list level's frame is hidden (render flag
 * 0x10), it first unhides its three records, rebuilds it with wldcore_bar_build_menu_rows,
 * publishes text 0xb806 and resets its main window. It then lists the 96
 * propositions whose script variable 0x360 + i has neither bit 1 nor 2 set,
 * whose g_main_proposition_states entry is 0 in its low six bits and which
 * wldcore_proposition_set_based_on_location accepts for the current location
 * (entry values 0xd000 + i). With entries it shows text 0xb80b and up to six
 * rows at (0x4c, 0x50), width 0x96, and pushes a type-7 level; otherwise it
 * resumes the parent, shows text 0xb80d and marks the parent's content
 * published. */
void wldcore_list_open_available_propositions(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s32 count;
    s32 i;
    s32 index;
    wldcore_window_record_t* record;
    s32 record_index;

    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.frame_render]
            .flags
        & 0x10) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .list_window.main_window]
            .flags &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .list_window.side_window]
            .flags &= ~0x10;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.frame_render]
            .flags &= ~0x10;
        wldcore_bar_build_menu_rows(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        wldcore_window_publish_slot_b806(&g_wldcore_menu_stack_records[g_wldcore_menu_stack_depth].list_window);
        index = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window;
        record = &g_wldcore_window_records[index];
        record->anim_counter = 0;
        record->frame_index = 0;
        g_wldcore_window_records[index].sequence = 0;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                            .list_window.frame_render]
            .flags |= 0x100;
    }
    count = 0;
    for (i = 0; i < 0x60; i++) {
        if (!(world_script_get_variable(i + 0x360) & 6) && !(g_main_proposition_states[i] & 0x3F)
            && wldcore_proposition_set_based_on_location(i, g_wldcore_map_projection_state.marker.kind + 1) != 0) {
            g_wldcore_list_entry_values[count] = i + 0xD000;
            g_wldcore_list_row_flags[count] = 0;
            count++;
        }
    }
    if (count != 0) {
        world_thread_set_parameters(0xE, 0x19, 0xB80B, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
        if (count < 6) {
            i = count;
        } else {
            i = 6;
        }
        dimensions.x = 0x96;
        origin.x = 0x4C;
        dimensions.y = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(3, count, i);
        wldcore_window_start_panel_render_thread(
            0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    } else {
        wldcore_menu_dispatch_resume_handler();
        world_thread_set_parameters(0xE, 0x19, 0xB80D, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.content_render = 1;
        return;
    }
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_result = 0x160;
    g_wldcore_menu_stack_records_next[record_index].panel.result = 0;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_AVAILABLE_PROPOSITIONS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
