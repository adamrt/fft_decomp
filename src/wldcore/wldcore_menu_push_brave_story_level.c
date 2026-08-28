#include "fft/main_runtime.h"
#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"

void wldcore_menu_build_brave_story_entries(void);

/* Pushes a list menu level (type 0x1C).
 *
 * Appends the main, upper, lower and side windows, draws one text row per
 * list entry (text entries[i] + 0xb83f) into a new render record, draws script
 * variables 0x61 and 0x62 (each clamped to 9999) into a second record with
 * text 0xb846, selects the play-time display, clamps the saved cursor row in
 * g_wldcore_brave_story_saved_cursor to the entry count and positions the windows relative to the list
 * record before committing the level at depth + 1.
 *
 * The unused 48-byte local reproduces the target's frame size. */
void wldcore_menu_push_brave_story_level(void) {
    wldcore_window_render_bounds16_t bounds;
    wldcore_point32_t point;
    s32 index;
    s32 value;
    s32 i;
    s32 depth;
    s32 temp;
    u8 unused[0x30];

    g_main_system_flags = g_main_system_flags & ~0x800;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window = index;
    g_wldcore_window_records[index].sequence = 0x79;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window = index;
    g_wldcore_window_records[index].sequence = 0x79;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = index;
    g_wldcore_window_records[index].sequence = 4;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    i = 0;
    wldcore_menu_build_brave_story_entries();

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.frame_render = index;
    bounds.position.x = 0;
    bounds.position.y = 0;
    bounds.dimensions.x = 0x5C;
    bounds.dimensions.y
        = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= 0x100;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = -0x78;
        base->y = -0x28;
    }

    point.x = 8;
    point.y = 8;
    for (; i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count; i++) {
        wldcore_menu_display_text_entry(index,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[i] + 0xB83F, point,
            g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.content_render = index;
    value = world_script_get_variable(EVENT_SCRIPT_VAR_INJURED);
    if (value >= 10000) {
        value = 9999;
    }
    g_world_text_substitution_values[0] = value;
    value = world_script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES);
    if (value >= 10000) {
        value = 9999;
    }
    g_world_text_substitution_values[1] = value;
    bounds.position.x = 0x80;
    bounds.position.y = 0x40;
    bounds.dimensions.x = 0x5C;
    bounds.dimensions.y = 0x30;
    wldcore_window_init_render_record_image(
        index, bounds.position, bounds.dimensions, 0, 0xB846, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    {
        wldcore_point32_t* base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
        base->x = 0x1C;
        base->y = -0x28;
    }

    g_wldcore_context_value_display_mode = 2;
    g_wldcore_displayed_numeric_value = g_main_system_play_time_hours;
    g_wldcore_displayed_numeric_value_secondary = g_main_system_play_time_minutes;
    g_wldcore_displayed_numeric_value_tertiary = g_main_system_play_time_seconds;

    depth = g_wldcore_menu_stack_depth;
    /* temp spans two blocks without crossing a call, so global allocation
     * gives the clamp flag and the upper window's y sum the target's v1. */
    temp = g_wldcore_menu_stack_records_next[depth].list_window.entry_count < g_wldcore_brave_story_saved_cursor[0] + 1;
    if (temp) {
        g_wldcore_brave_story_saved_cursor[0] = 0;
    }
    g_wldcore_menu_stack_records_next[depth].list_window.selected_entry = g_wldcore_brave_story_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].list_window.mode = 0;
    index = g_wldcore_menu_stack_records_next[depth].list_window.frame_render;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].x
        = g_wldcore_window_render_records[index].base_x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].y
        = g_wldcore_window_render_records[index].base_y - g_wldcore_brave_story_saved_cursor[0] * -0x10 + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.side_window].x
        = g_wldcore_window_render_records[index].base_x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.side_window].y
        = g_wldcore_window_render_records[index].base_y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].x
        = g_wldcore_window_render_records[index].base_x + 0x4C;
    {
        s32 base_y = g_wldcore_window_render_records[index].base_y;

        temp = g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].y + base_y;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].y = temp;
    }
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.lower_window].x
        = g_wldcore_window_render_records[index].base_x + 0x30;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.lower_window].y
        += g_wldcore_window_render_records[index].base_y;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_BRAVE_STORY;
}
