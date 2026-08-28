#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Push list-window menu level 0x25.
 *
 * Entry 0 is listed when g_main_save_proposition_count is nonzero and entry 1
 * when any script variable 0x360-0x3bf has bit 2 set; entries draw text
 * 0xb864 + entry. The cursor is restored from g_wldcore_proposition_category_saved_cursor (reset when out of
 * range). `value` holds the cursor and then the reloaded main-window index:
 * as one multi-set variable it is left to global allocation, which gives the
 * target's v1/v0 assignment in the tail. `unused` reproduces the 0x58 frame.
 */
void wldcore_menu_push_proposition_category_level(void) {
    wldcore_point32_t point;
    wldcore_window_render_bounds16_t bounds;
    s32 unused[4];
    wldcore_point32_t* base;
    s32 index;
    s32 i;
    s32 completed;
    s32 depth;
    s32 value;

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = index;
    g_wldcore_window_records[index].sequence = 2;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 0;
    completed = g_main_save_proposition_count;
    if (completed != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0] = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }
    completed = 0;
    for (i = 0; i < 0x60; i++) {
        if (world_script_get_variable(i + 0x360) & 4) {
            completed++;
        }
    }
    if (completed != 0) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
            .list_window.entries[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count]
            = 1;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count++;
    }

    index = wldcore_window_append_render_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = index;
    bounds.position.x = 0x80;
    bounds.position.y = 0x80;
    bounds.dimensions.x = 0x4C;
    bounds.dimensions.y
        = g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count * 0x10 + 0x10;
    wldcore_window_build_render_record_image(
        index, bounds.position, bounds.dimensions, 0, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 9;
    g_wldcore_window_render_records[index].flags |= 0x100;
    base = (wldcore_point32_t*)&g_wldcore_window_render_records[index].base_x;
    base->x = -0x78;
    base->y = -0x28;
    point.x = 8;
    point.y = 8;
    for (i = 0; i < g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count; i++) {
        wldcore_menu_display_text_entry(index,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[i] + 0xB864, point,
            g_wldcore_window_image_buffer);
        point.y += 0x10;
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_window_image_buffer);

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.main_window].flags |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.side_window].flags |= 0x10;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.frame_render].flags
        |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.upper_window].flags |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].list_window.lower_window].flags |= 0x10;
    if (g_wldcore_menu_stack_records_next[depth].list_window.entry_count
        < g_wldcore_proposition_category_saved_cursor[0] + 1) {
        g_wldcore_proposition_category_saved_cursor[0] = 0;
    }
    value = g_wldcore_proposition_category_saved_cursor[0];
    g_wldcore_menu_stack_records_next[depth].list_window.selected_entry = value;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].list_window.main_window].x
        = g_wldcore_window_render_records[index].base_x + 6;
    g_wldcore_menu_stack_depth = depth + 1;
    value = g_wldcore_menu_stack_records_next[depth].list_window.main_window;
    g_wldcore_window_records[value].y = g_wldcore_window_render_records[index].base_y
        - -(g_wldcore_proposition_category_saved_cursor[0] * 0x10) + 0xE;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_CATEGORY;
}
