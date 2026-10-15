#include "fft/wldcore.h"

/* Pushes the two-row confirmation panel (text 0xb805) whose render record
 * sits at origin; wldcore_proposition_handle_dispatch_confirm_input services the level.
 *
 * The origin arrives by value: the target spills a0/a1 to their home slots at
 * entry and copies both 32-bit values into base_x/base_y together. The frame reserves
 * 40 bytes of locals the body never reads. */
void wldcore_window_build_yes_no_panel(point32_t origin) {
    RECT bounds;
    s32 dead_locals[10];
    s32 index;
    s32 depth;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.header_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].choice_panel.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_OK_NO, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].base = origin;

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].choice_panel.row = 0;
    g_wldcore_menu_stack_records_next[depth].choice_panel.mode = WLDCORE_ACCEPT_CONFIRM_MODE_CHOOSE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y
        - -(g_wldcore_menu_stack_records_next[depth].choice_panel.row * 0x10 + 0xE);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].choice_panel.header_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
}
