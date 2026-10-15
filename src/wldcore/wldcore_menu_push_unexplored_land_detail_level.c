#include "fft/thread.h"
#include "fft/wldcore.h"

/* Pushes the unexplored-land detail text window as menu level 0x19. Records
 * the land on the new level, loads that land's picture data set, appends a
 * render record for the picture and places it at (-116, -34), decodes the
 * land's discovery date out of the packed nine-bit array at
 * g_main_land_discovery_date_bits (date.month month 1-12, date.day day 1-31), and
 * sets up the shared scrollable text window at g_wldcore_scrollable_text_window for text
 * 0x8800 + land with the month name (0xb84f + month) and the day as its two
 * substitutions. Then hides the parent list-window level's four window records
 * and its frame render record with flag 0x10 and starts WORLD thread 14 on
 * text 0xb849 with substitution value 0xd800 + land.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The render record's position is written through a pointer to its base_x: the
 * target reaches base_x/base_y at 0 and 4 off one register holding
 * &g_wldcore_window_render_records[index].base_x, while the flags update above
 * it still uses the $at absolute form. Spelling both coordinates as ordinary
 * indexed field stores recomputes the $at base twice and costs four
 * instructions. The cast is over adjacent s32 base_x/base_y; the header would
 * express it better as a point32_t member.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
void wldcore_menu_push_unexplored_land_detail_level(s32 value) {
    s32 index;
    point32_t* base;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    wldcore_proposition_load_picture_data_set(g_wldcore_unexplored_land_picture_sets[value]);
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x74;
    base->y = -0x22;
    wldcore_read_nine_bit_record(g_main_land_discovery_date_bits, value, &date);
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 0xa;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = date.month + WLDCORE_TEXT_ID_MONTH_NAME_BASE;
    g_wldcore_scrollable_text_window.text_substitutions[1] = date.day;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.phase = WLDCORE_DETAIL_PHASE_LOAD;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_UNEXPLORED_LAND_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
