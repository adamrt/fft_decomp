#include "fft/wldcore.h"

/* Opens a panel list of the 64 entries whose location mask (word table at
 * g_wldcore_rumor_location_masks) includes the current location and whose script variable
 * 0x2d1 + index is set (entry values 0xc800 + index); shows up to eight rows
 * at (0x4c, 0x50) with width 0x96 and appends its record tagged 6. With no
 * entries it resets the parent window-pair level's records and shows text
 * 0xb80a instead. */
void wldcore_list_open_rumors(void) {
    wldcore_point32_t dimensions;
    wldcore_point32_t origin;
    s16* values;
    s16* flags;
    s32* locations;
    s32 mask;
    s32 entry;
    s32 count;
    s32 i;
    s32 record_index;
    s32 unused; /* reproduces the target's 0x48-byte frame */

    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    locations = g_wldcore_rumor_location_masks;
    mask = 0x4000 >> g_wldcore_map_projection_state.marker.kind;
    for (; i < 64; i++, locations++) {
        /* A register operand keeps the loaded word first in the `and`;
         * testing *locations directly lets expand_binop swap them. */
        entry = *locations;
        if ((entry & mask) && world_script_get_variable(i + 0x2D1) != 0) {
            *values = i - 0x3800;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    if (count != 0) {
        g_main_system_flags |= 0x800;
        world_thread_set_parameters(0xE, 0x19, 0xB808, 0);
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
        wldcore_list_clamp_cursor_state(2, count, i);
        wldcore_window_start_panel_render_thread(
            0xC, g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &dimensions, &origin);
    } else {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .palette = g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                    .window_pair_render.second_window]
                           .palette
            = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                  .window_pair_render.render_index]
                  .palette = 0;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .sequence = 2;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .frame_index = g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                        .window_pair_render.first_window]
                               .anim_counter = 0;
        world_thread_set_parameters(0xE, 0x19, 0xB80A, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_pair_render._unknown_0c = 1;
        return;
    }
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_menu_result = 0x160;
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_RUMORS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
