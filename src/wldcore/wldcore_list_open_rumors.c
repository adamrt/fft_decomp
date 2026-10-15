#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Opens a panel list of the 64 entries whose location mask (32-bit table at
 * g_wldcore_rumor_location_masks) includes the current location and whose script variable
 * 0x2d1 + index is set (entry values 0xc800 + index); shows up to eight rows
 * at (0x4c, 0x50) with width 0x96 and appends its record tagged 6. With no
 * entries it resets the parent window-pair level's records and shows text
 * 0xb80a instead. */
void wldcore_list_open_rumors(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s16* values;
    s16* flags;
    s32* locations;
    s32 mask;
    s32 entry;
    s32 count;
    s32 i;
    s32 record_index;

    count = 0;
    i = 0;
    flags = g_wldcore_list_row_flags;
    values = g_wldcore_list_entry_values;
    locations = g_wldcore_rumor_location_masks;
    mask = 0x4000 >> g_wldcore_map_projection_state.marker.kind;
    for (; i < EVENT_SCRIPT_VAR_RUMOR_COUNT; i++, locations++) {
        /* A register operand keeps the loaded 32-bit value first in the `and`;
         * testing *locations directly lets expand_binop swap them. */
        entry = *locations;
        if ((entry & mask) && script_get_variable(i + EVENT_SCRIPT_VAR_RUMOR_FIRST) != 0) {
            *values = i + (s16)TEXT_ID_RUMOR_NAME_BASE;
            *flags = 0;
            flags++;
            values++;
            count++;
        }
    }
    if (count != 0) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RUMORS_INTRO, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
        if (count < 8) {
            i = count;
        } else {
            i = 8;
        }
        layout.text_width = 0x96;
        origin.x = 0x4C;
        layout.visible_rows = i;
        origin.y = 0x50;
        wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_RUMORS, count, i);
        wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
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
            .sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair_render.first_window]
            .frame_index = g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                                        .window_pair_render.first_window]
                               .anim_counter = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_NO_RUMORS, 0);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].window_pair_render.panel_rows = 1;
        return;
    }
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_RUMORS;
    g_wldcore_menu_stack_depth = record_index + 1;
}
