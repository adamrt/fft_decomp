#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Pushes the shared scrollable text window as menu level 0x1e for a story
 * event entry, the partner of the wldcore_menu_wait_tutorial_topic_text_then_pop_level pop: records the entry on the
 * new level, sets up the window at g_wldcore_scrollable_text_window for text 0x8800 + entry with no
 * substitutions, then hides the parent list-window level's four window records
 * and its frame render record with flag 0x10 and starts WORLD thread 14 on
 * text 0xb849 with substitution value 0xe800 + entry.
 *
 * Masked-identical to wldcore_menu_push_script_flag_01a4_detail_level across all 165 instructions.
 * The two differ only in the substitution addend (0x9800 there, 0xe800 here),
 * the menu level type (0x2f there, 0x1e here) and the frame size; the text id
 * is value + 0x8800 in both, so 0xe800 is the substitution value and not a
 * text id.
 *
 * Addressing follows wldcore_menu_push_proposition_attempt_detail_level and
 * wldcore_menu_push_tutorial_topic_text_level: the target holds one base of g_wldcore_scrollable_text_window+0x34 (its
 * text_id) and reaches the earlier fields at negative displacements, so the original held a pointer biased to that
 * field. Laundering the anchor keeps it opaque to cse, which otherwise rewrites every displacement back to the $at
 * absolute form after the first two stores. image_y is the one field the target still stores absolutely, so it is
 * written as g_wldcore_scrollable_text_window.image_y.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_wldcore_menu_stack_depth after each call.
 *
 * The frame is 32 bytes against the twin's 24: the target reserves eight bytes
 * below its two saved registers that it never touches, reproduced by the
 * unused local. */
void wldcore_menu_push_story_event_text_level(s32 value) {
    s32 unused[2];
    wldcore_text_scrollable_window_t* window;
    s32* anchor;

    anchor = &g_wldcore_scrollable_text_window.text_id;
    /* Keeps the biased anchor opaque to cse (see above). */
    __asm__("" : "=r"(anchor) : "0"(anchor));

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = value;

    window = (wldcore_text_scrollable_window_t*)(anchor - 13);

    window->text_id = value + 0x8800;
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
    g_world_text_substitution_values[0] = value + 0xe800;
    world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
