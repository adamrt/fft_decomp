#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Pushes the tutorial-category list level (type 0x2b).
 *
 * Appends a cursor window, a header window and the frame render record, seeds
 * the four category entries, and positions both windows relative to the render
 * record's origin. With `reset` set it also clears the two 32-bit tutorial
 * progress masks at g_main_tutorial_progress_masks.
 *
 * The 40 bytes of `dead_locals` reproduce the target's 0x50 frame: the
 * original declares locals it never reads, and GCC still reserves their slots.
 * The `- -(...)` in the cursor-row Y expression only flips the commutative
 * operand order of the final `addu`; it is arithmetically identical. */
void wldcore_list_open_tutorial_categories(s32 selected_entry, s32 reset) {
    s32 dead_locals[10];
    s32 window_index;
    s32 record_index;

    if (reset != 0) {
        g_main_tutorial_progress_masks[1] = 0;
        g_main_tutorial_progress_masks[0] = 0;
    }

    window_index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.main_window = window_index;
    g_wldcore_window_records[window_index].sequence = 2;
    g_wldcore_window_records[window_index].priority = 0xA;
    g_wldcore_window_records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;

    window_index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.side_window = window_index;
    g_wldcore_window_records[window_index].sequence = 4;
    g_wldcore_window_records[window_index].priority = 0xA;
    g_wldcore_window_records[window_index].anim_counter = 0;
    g_wldcore_window_records[window_index].frame_index = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[0] = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[1] = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[2] = 2;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entries[3] = 3;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.entry_count = 4;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.frame_render
        = wldcore_window_append_render_record_and_reset_color(
            g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    wldcore_list_build_tutorial_category_panel_image(
        &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].text_list);

    record_index = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[record_index].list_window.selected_entry = selected_entry;
    g_wldcore_menu_stack_records_next[record_index].list_window.mode = 0;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.main_window].x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base_x
        + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.main_window].y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base_y
        - -(g_wldcore_menu_stack_records_next[record_index].list_window.selected_entry * 0x10) + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.side_window].x
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base_x
        + 3;
    g_wldcore_menu_stack_depth = record_index + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[record_index].list_window.side_window].y
        = g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[record_index].list_window.frame_render]
              .base_y
        - 2;
    g_wldcore_menu_stack_records_next[record_index].list_window.content_render = 0x18;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TUTORIAL_CATEGORIES;
}
