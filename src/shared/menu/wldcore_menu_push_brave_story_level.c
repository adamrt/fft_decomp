#include "fft/script.h"
#include "fft/wldcore.h"

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
    RECT bounds;
    point32_t point;
    s32 index;
    s32 value;
    s32 i;
    s32 depth;
    s32 temp;
    u8 unused[0x30];

    g_main_system_flags = g_main_system_flags & ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.upper_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_BRAVE_STORY_MARKER;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.lower_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_BRAVE_STORY_MARKER;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    i = 0;
    wldcore_menu_build_brave_story_entries();

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.frame_render = index;
    bounds.x = 0;
    bounds.y = 0;
    bounds.w = 0x5C;
    bounds.h = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = -0x78;
        base->y = -0x28;
    }

    point.x = 8;
    point.y = 8;
    for (; i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count; i++) {
        window_display_text_entry_variant(index,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[i]
                + WLDCORE_TEXT_ID_BRAVE_STORY_ROW_FIRST,
            point, g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.content_render = index;
    value = script_get_variable(EVENT_SCRIPT_VAR_INJURED);
    if (value >= 10000) {
        value = 9999;
    }
    g_text_substitution_values[0] = value;
    value = script_get_variable(EVENT_SCRIPT_VAR_CASUALTIES);
    if (value >= 10000) {
        value = 9999;
    }
    g_text_substitution_values[1] = value;
    bounds.x = 0x80;
    bounds.y = 0x40;
    bounds.w = 0x5C;
    bounds.h = 0x30;
    window_init_text_record_variant(
        index, bounds, 0, WLDCORE_TEXT_ID_INJURED_CASUALTIES, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    {
        point32_t* base = &g_wldcore_window_render_records[index].base;
        base->x = 0x1C;
        base->y = -0x28;
    }

    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_PLAY_TIME;
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
    g_wldcore_menu_stack_records_next[depth].list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
    index = g_wldcore_menu_stack_records_next[depth].list_window.frame_render;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].position.y
        = g_wldcore_window_render_records[index].base.y - g_wldcore_brave_story_saved_cursor[0] * -0x10 + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.side_window].position.x
        = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.side_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0x4C;
    {
        s32 base_y = g_wldcore_window_render_records[index].base.y;

        temp = g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.y
            + base_y;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.upper_window].position.y = temp;
    }
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.lower_window].position.x
        = g_wldcore_window_render_records[index].base.x + 0x30;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.lower_window].position.y
        += g_wldcore_window_render_records[index].base.y;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_BRAVE_STORY;
}
