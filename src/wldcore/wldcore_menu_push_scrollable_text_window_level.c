#include "fft/wldcore.h"

/* Pushes the wide scrollable text window (0x8009eef8) as menu level type
 * 0x33: it appends one window record for the frame, measures the text, sizes
 * the window around it (at most four rows), initialises the scrollable text
 * state, hides the text render record and the parent level's window, and sets
 * system flags 0x08002000.
 *
 * Like the type-2 push, the target keeps &g_wldcore_scrollable_text_window.text_id in a register and
 * reaches image_x at -0x34 from it, so the window pointer is laundered out of
 * that anchor.
 *
 * The +0x20 word of the new level has no catalogued struct for type 0x33;
 * wldcore_menu_event_transition_level_t.mode is the only named field at that offset. */
void wldcore_menu_push_scrollable_text_window_level(s32 text) {
    s16 dims[2];
    s32 unused[4];
    register s32* anchor __asm__("$6");
    s32 index;
    s32 depth;
    u8* entry;
    /* Pin required: unpinned, the argument registers holding `anchor`, `three`
     * and the depth permute. */
    register s32 three __asm__("$7");
    s32 w;
    s32 h;

    entry = world_text_find_entry(text);
    world_text_measure_pixels(&dims[0], &dims[1], entry);
    if (dims[1] > 4) {
        dims[1] = 4;
    }

    index = wldcore_window_append_record_and_reset_color(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    w = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[w].window_index = index;
    g_wldcore_window_records[index].sequence = 0xC;
    three = 3;
    g_wldcore_window_records[index].priority = three;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= 0x10;

    anchor = &g_wldcore_scrollable_text_window.text_id;
    /* Hides the anchor from cse so image_x stays at -0x34 from it. */
    __asm__("" : "=r"(anchor) : "0"(anchor));

    *anchor = text;
    anchor[-13] = 0;
    w = dims[0];
    h = dims[1];
    g_wldcore_scrollable_text_window.image_y = 0x60;
    g_wldcore_scrollable_text_window.priority = three;
    g_wldcore_scrollable_text_window.image_coordinate_mode = 0;
    g_wldcore_scrollable_text_window.base.x = -(((w + 24) & 0xFFFC) >> 1);
    g_wldcore_scrollable_text_window.base.y = -((h << 3) + 8);
    /* Keeps the -1 stores below from being hoisted above the base stores. */
    __asm__ volatile("");
    g_wldcore_scrollable_text_window.text_width = w;
    g_wldcore_scrollable_text_window.rows_per_page = h;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
    wldcore_text_init_scrollable_window((wldcore_text_scrollable_window_t*)(anchor - 13));

    g_wldcore_window_render_records[g_wldcore_scroll_text_render_record_index].flags |= 0x10;
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_index].x
        = g_wldcore_scrollable_text_window.base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].window_index].y
        = g_wldcore_scrollable_text_window.base.y - 2;
    g_main_system_flags |= 0x08002000;
    g_wldcore_menu_stack_depth = depth + 1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 1].window_index].flags |= 0x10;
    g_wldcore_menu_stack_records_next[depth].event_transition.mode = 0x28;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SCROLLABLE_TEXT_WINDOW;
}
