#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"

void wldcore_read_nine_bit_record(u32* bits, s32 record, u8* dest);

/* Pushes the unexplored-land detail text window as menu level 0x19. Records
 * the land on the new level, loads that land's picture data set, appends a
 * render record for the picture and places it at (-116, -34), decodes the
 * land's discovery date out of the packed nine-bit array at
 * g_main_land_discovery_date_bits (date[0] month 1-12, date[1] day 1-31), and
 * sets up the shared scrollable text window at g_wldcore_scrollable_text_window for text
 * 0x8800 + land with the month name (0xb84f + month) and the day as its two
 * substitutions. Then hides the parent list-window level's four window records
 * and its frame render record with flag 0x10 and starts WORLD thread 14 on
 * text 0xb849 with substitution value 0xd800 + land.
 *
 * Addressing follows
 * wldcore_menu_push_proposition_attempt_detail_level: the target holds one base
 * of g_wldcore_scrollable_text_window+0x34 (its text_id) and reaches the earlier fields at negative
 * displacements, so the original held a pointer biased to that field.
 * Laundering the anchor keeps it opaque to cse, which otherwise rewrites every
 * displacement back to the $at absolute form; image_y is the one field the
 * target still stores absolutely.
 *
 * The render record's position is written through a second biased pointer: the
 * target reaches base_x/base_y at 0 and 4 off one register holding
 * &g_wldcore_window_render_records[index].base_x, while the flags update above
 * it still uses the $at absolute form. Spelling both coordinates as ordinary
 * indexed field stores recomputes the $at base twice and costs four
 * instructions. The cast is over adjacent s32 base_x/base_y; the header would
 * express it better as a wldcore_point32_t member. */
void wldcore_menu_push_unexplored_land_detail_level(s32 value) {
    wldcore_text_scrollable_window_t* window;
    s32* anchor;
    s32 index;
    wldcore_point32_t* base;
    s32 month;
    s32 day;
    u8 unused[8];
    u8 date[2];

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    wldcore_proposition_load_picture_data_set(g_wldcore_unexplored_land_picture_sets[value]);
    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags |= 0x10;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x74;
    base->y = -0x22;
    wldcore_read_nine_bit_record(g_main_land_discovery_date_bits, value, date);

    /* Opaque to cse so the stores keep their negative displacements. */
    anchor = &g_wldcore_scrollable_text_window.text_id;
    __asm__("" : "=r"(anchor) : "0"(anchor));
    window = (wldcore_text_scrollable_window_t*)(anchor - 13);

    window->text_id = value + 0x8800;
    window->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0x70;
    window->base.x = -122;
    window->base.y = -40;
    window->text_width = 0xdc;
    window->rows_per_page = 8;
    window->priority = 0xa;
    window->image_coordinate_mode = 1;
    window->extra_render_index = -1;
    month = date[0];
    day = date[1];
    window->text_substitutions[0] = month + 0xb84f;
    window->text_substitutions[1] = day;
    wldcore_text_init_scrollable_window(window);
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
    g_world_text_substitution_values[0] = value + 0xd800;
    world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.phase = 1;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_UNEXPLORED_LAND_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
