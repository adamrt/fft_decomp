#include "fft/wldcore.h"

/* Pushes the tutorial topic text window as menu level 0x22: records the topic
 * id on the new level, sets up the shared scrollable text window at g_wldcore_scrollable_text_window
 * for that topic's text (0x8800 + topic), and hides the parent pair-render
 * level's two windows and its render record with flag 0x10. wldcore_menu_wait_tutorial_topic_text_then_pop_level
 * steps the window and pops the level, restoring all three.
 *
 * The target addresses the window record from a base biased to its text_id
 * (+0x34), with negative displacements for the earlier fields; image_y is the
 * one field it stores absolutely. The menu-stack type store comes last so the
 * compiler scales depth by four late and in place, forcing the copy into v0
 * for depth + 1. */
void wldcore_menu_push_tutorial_topic_text_level(s32 topic) {
    wldcore_text_scrollable_window_t* window;
    wldcore_menu_stack_record_t* record;
    s32* anchor;
    s32 depth;

    record = &g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth];
    /* Keeps the anchor load after the depth load. */
    __asm__ volatile("");
    anchor = &g_wldcore_scrollable_text_window.text_id;
    /* Hides the anchor from cse, which would fold the biased stores back to absolute ones. */
    __asm__("" : "=r"(anchor) : "0"(anchor));
    window = (wldcore_text_scrollable_window_t*)(anchor - 13);
    record->window_index = topic;

    window->text_id = topic + 0x8800;
    window->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0x70;
    window->base.x = -122;
    window->base.y = -40;
    window->text_width = 0xdc;
    window->rows_per_page = 8;
    window->priority = 9;
    window->image_coordinate_mode = 1;
    window->extra_render_index = -1;
    window->text_substitutions[1] = -1;
    window->text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window(window);
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.first_window].flags
        |= 0x10;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.second_window].flags
        |= 0x10;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 1].window_pair_render.render_index].flags
        |= 0x10;
    g_wldcore_menu_stack_types_next[depth] = WLDCORE_MENU_LEVEL_TUTORIAL_TOPIC_TEXT;
}
