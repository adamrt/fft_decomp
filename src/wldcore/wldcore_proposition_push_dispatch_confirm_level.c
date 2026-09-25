#include "fft/wldcore.h"

/* Pushes menu level type 0xd: a cursor window, an upper window, a 0x28x0x30
 * text record (0xb805) and a text record listing text 0xb84e, the location
 * name (0xf7ff + g_wldcore_selected_proposition_row[0].fields.location), text 0xb821 and
 * g_wldcore_proposition_send_unit_count rows of text 0x4000 + g_wldcore_proposition_send_units[i]. World thread 14
 * shows message 0xb814; both windows are placed against the first text record.
 *
 * `unused` reproduces the target's 0x78-byte frame. The row offset uses its
 * own local: reusing `row` (then set twice) schedules the loop's `addiu v1`
 * ahead of `move a0`. */
void wldcore_proposition_push_dispatch_confirm_level(void) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s16 size[2];
    s32 unused[8];
    wldcore_point32_t* position;
    wldcore_point32_t* base;
    u16* location;
    s32 index;
    s32 depth;
    s32 i;
    s32 row;
    s32 line;

    world_thread_set_parameters(0xE, 0x19, 0xB814, 0);

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window = index;
    g_wldcore_window_records[index].sequence = 0xC;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    bounds.position.x = 0x80;
    bounds.dimensions.x = 0x28;
    bounds.dimensions.y = 0x30;
    bounds.position.y = 0;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0xB805, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    position = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    position->x = 0x48;
    position->y = 0x20;

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail._unknown_08 = index;
    location = &g_wldcore_selected_proposition_row[0].fields.location;
    world_text_measure_pixels(&size[0], &size[1], world_text_find_entry(*location + 0xF7FF));
    bounds.position.y = 0x60;
    bounds.dimensions.x = 0xF4;
    bounds.position.x = 0;
    row = size[1] + 2;
    bounds.dimensions.y = (g_wldcore_proposition_send_unit_count - -row) * 0x10 + 0x10;
    g_world_text_substitution_values[1] = g_wldcore_proposition_dispatch_days;
    g_world_text_substitution_values[0] = g_wldcore_proposition_dispatch_days;
    g_world_text_substitution_values[2] = g_wldcore_map_projection_state.marker.kind + 0x9000;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 0, &g_wldcore_window_image_buffer[0x200]);
    point.x = 8;
    point.y = size[1] * 0x10 + 8;
    wldcore_menu_display_text_entry(index, 0xB821, point, &g_wldcore_window_image_buffer[0x200]);
    point.x = 8;
    point.y = 8;
    wldcore_menu_display_text_entry(index, 0xB84E, point, &g_wldcore_window_image_buffer[0x200]);
    point.x = 0x44;
    point.y = 8;
    wldcore_menu_display_text_entry(index, *location + 0xF7FF, point, &g_wldcore_window_image_buffer[0x200]);
    for (i = 0; i < g_wldcore_proposition_send_unit_count; i++) {
        point.x = 0x3C;
        line = i + 2;
        point.y = (size[1] + line) * 0x10 + 8;
        wldcore_menu_display_text_entry(
            index, g_wldcore_proposition_send_units[i] + 0x4000, point, &g_wldcore_window_image_buffer[0x200]);
    }
    wldcore_window_load_image_record_to_vram(index, &g_wldcore_window_image_buffer[0x200]);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= 0x100;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x7A;
    base->y = -0x28;

    depth = g_wldcore_menu_stack_depth;
    index = g_wldcore_menu_stack_records_next[depth].variable_detail.render_index;
    g_wldcore_menu_stack_records_next[depth].variable_detail._unknown_08 = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].variable_detail.value].x
        = g_wldcore_window_render_records[index].base_x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].variable_detail.value].y
        = g_wldcore_window_render_records[index].base_y
        - -(g_wldcore_menu_stack_records_next[depth].variable_detail._unknown_08 * 0x10 + 0xE);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].x
        = g_wldcore_window_render_records[index].base_x + 3;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_DISPATCH_CONFIRM;
}
