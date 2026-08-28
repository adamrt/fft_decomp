#include "fft/wldcore.h"

/* Pushes the two-row confirmation panel (text 0xb805) whose render record
 * sits at origin; wldcore_proposition_handle_dispatch_confirm_input services the level.
 *
 * The origin arrives by value: the target spills a0/a1 to their home slots at
 * entry and copies both words into base_x/base_y together. The frame reserves
 * 40 bytes of locals the body never reads, and the last four arguments are
 * unused. */
void wldcore_window_build_yes_no_panel(wldcore_point32_t origin, s32 unused_2, s32 unused_3, s32 x2, s32 y2) {
    wldcore_window_render_bounds16_t bounds;
    s32 dead_locals[10];
    s32 index;
    s32 depth;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.cursor_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.header_window = index;
    g_wldcore_window_records[index].sequence = 0xC;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.render_index = index;
    bounds.position.x = 0x80;
    bounds.dimensions.x = 0x28;
    bounds.dimensions.y = 0x30;
    bounds.position.y = 0;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0xB805, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    *(wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x = origin;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].choice_panel.row = 0;
    g_wldcore_menu_stack_records_next[depth].choice_panel.mode = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].x
        = g_wldcore_window_render_records[index].base_x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].y
        = g_wldcore_window_render_records[index].base_y
        - -(g_wldcore_menu_stack_records_next[depth].choice_panel.row * 0x10 + 0xE);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].x
        = g_wldcore_window_render_records[index].base_x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
}
