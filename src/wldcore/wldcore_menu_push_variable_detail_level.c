#include "fft/wldcore.h"

/* Opens the detail view (menu level type 0x20) for a script-variable list
 * entry selected in 0x8008368c: value is variable index * 16 + variable
 * value. It loads the chapter data of the variable's group of 16, appends a
 * render record, hides the parent list window's records and starts WORLD
 * thread 14 with text 0xb868 (variable 0, value substituted) or 0xb849.
 *
 * The unused 48-byte local reproduces the target's frame size. */
void wldcore_menu_push_variable_detail_level(s32 value) {
    u8 unused[0x30];
    s32 variable;
    s32 index;
    s32 depth;

    variable = value >> 4;
    wldcore_load_message_block_if_changed(variable / 16 + 6);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags = (g_wldcore_window_render_records[index].flags & ~4) | 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= 0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= 0x10;
    if (variable == 0) {
        g_world_text_substitution_values[0] = (value & 0xF) + 0xB869;
        world_thread_set_parameters(0xE, 0x19, 0xB868, 0);
    } else {
        g_world_text_substitution_values[0] = value + 0xF000;
        world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].variable_detail.phase = 1;
    g_wldcore_menu_stack_records_next[depth].variable_detail.timer = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_VARIABLE_DETAIL;
    g_wldcore_menu_stack_depth = depth + 1;
}
