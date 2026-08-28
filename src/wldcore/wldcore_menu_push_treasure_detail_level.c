#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"

void wldcore_read_nine_bit_record(u32* bits, s32 record, u8* dest);

/* Pushes the treasure detail text window as menu level 0x1b; the twin of
 * wldcore_menu_push_unexplored_land_detail_level (unexplored lands, level 0x19) and
 * wldcore_menu_push_story_event_text_level (level 0x1e).
 *
 * Records the treasure on the new level, loads its picture data set from
 * g_wldcore_treasure_picture_sets, appends a render record for the picture and hides it with flag
 * 0x10, decodes the acquisition date out of the packed nine-bit array at
 * g_main_treasure_acquisition_date_bits (date[0] month, date[1] day) and sets
 * up the shared scrollable text window at g_wldcore_scrollable_text_window for text 0x8800 + value
 * with the month name (0xb84f + month) and the day as its two substitutions.
 * It then hides the parent list-window level's four window records and its
 * frame render record and starts WORLD thread 14 on text 0xb849 with
 * substitution value 0xe000 + value.
 *
 * Unlike wldcore_menu_push_unexplored_land_detail_level this one does not reposition the picture render
 * record, and it clears the new level's timer as well as setting its
 * phase to 1.
 *
 * Addressing follows wldcore_menu_push_unexplored_land_detail_level: the target holds one base of
 * g_wldcore_scrollable_text_window+0x34 (its text_id) and reaches the earlier fields at negative
 * displacements, so the anchor is laundered to keep it opaque to cse;
 * image_y is the one field the target still stores absolutely. */
void wldcore_menu_push_treasure_detail_level(s32 value) {
    wldcore_text_scrollable_window_t* record;
    s32* anchor;
    s32 index;
    s32 month;
    s32 day;
    u8 unused[8];
    u8 date[2];

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    wldcore_proposition_load_picture_data_set(g_wldcore_treasure_picture_sets[value]);
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags |= 0x10;
    wldcore_read_nine_bit_record(g_main_treasure_acquisition_date_bits, value, date);

    anchor = &g_wldcore_scrollable_text_window.text_id;
    __asm__("" : "=r"(anchor) : "0"(anchor));
    record = (wldcore_text_scrollable_window_t*)(anchor - 13);

    record->text_id = value + 0x8800;
    record->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0x70;
    record->base.x = -122;
    record->base.y = -40;
    record->text_width = 0xdc;
    record->rows_per_page = 8;
    record->priority = 0xa;
    record->image_coordinate_mode = 1;
    record->extra_render_index = -1;
    month = date[0];
    day = date[1];
    record->text_substitutions[0] = month + 0xb84f;
    record->text_substitutions[1] = day;
    wldcore_text_init_scrollable_window(record);
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
    g_world_text_substitution_values[0] = value + 0xe000;
    world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.phase = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.timer = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_TREASURE_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
