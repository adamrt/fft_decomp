#include "fft/wldcore.h"

void wldcore_menu_render_debug_variable_list(wldcore_menu_variable_list_level_t* level);

/* Pushes the debug script-variable list level (type 0xb): one cursor window
 * record and one render record, laid out by 0x8006ed30 and then positioned
 * from the render record's origin.
 *
 * The target passes a third argument to
 * wldcore_window_append_render_record_and_reset_color, which the callee
 * ignores; the cast reproduces that call without widening the prototype.
 * Both appended indices share one local, which is what holds them in $a2.
 * The window position is written after 0x8006ed30 returns, from the render
 * record origin the callee filled in, so the level fields are read back out
 * of the stack record rather than kept in locals; the `- -` keeps the row
 * offset's constant on the multiply chain, which a plain `+` reassociates
 * onto base_y. */
void wldcore_menu_push_variable_list_level(void) {
    s32 index;
    s32 record_index;
    u8 unused_locals[0x10];

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list.window_index = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 3;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = ((s32 (*)(u32**, s32*, s32))wldcore_window_append_render_record_and_reset_color)(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count, index);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list.render_index = index;
    g_wldcore_window_render_records[index].flags |= 0x100;
    g_wldcore_window_render_records[index].priority = 3;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list.first_variable = 0;
    wldcore_menu_render_debug_variable_list(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_list);
    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].variable_list.cursor = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].variable_list.window_index].x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].variable_list.render_index]
              .base_x
        + 8;
    g_wldcore_menu_stack_depth = record_index + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].variable_list.window_index].y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].variable_list.render_index]
              .base_y
        - -(g_wldcore_menu_stack_records_next[record_index].variable_list.cursor * 0x10 + 0xC);
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_VARIABLE_LIST;
}
