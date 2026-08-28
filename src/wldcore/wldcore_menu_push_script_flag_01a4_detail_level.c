#include "fft/thread.h"
#include "fft/wldcore.h"
#include "fft/world.h"

/* Pushes the script-variable detail text window as menu level 0x2f, the
 * partner of the wldcore_menu_close_script_flag_01a4_detail_level pop: records the entry on the new level, sets
 * up the shared scrollable text window at g_wldcore_scrollable_text_window for text 0x8800 + entry
 * with no substitutions, then hides the parent list-window level's four
 * window records and its frame render record with flag 0x10 and starts WORLD
 * thread 14 on text 0xb849 with substitution value 0x9800 + entry.
 *
 * Addressing follows wldcore_menu_push_proposition_attempt_detail_level and
 * wldcore_menu_push_tutorial_topic_text_level: the target holds one base of g_wldcore_scrollable_text_window+0x34 (its
 * text_id) and reaches the earlier fields at negative displacements, so the original held a pointer biased to that
 * field. Laundering the anchor keeps it opaque to cse, which otherwise rewrites every displacement back to the $at
 * absolute form after the first two stores. image_y is the one field the target still stores absolutely, so it is
 * written as g_wldcore_scrollable_text_window.image_y. `record` is a non-opaque alias of the biased address, so the
 * stores fold onto the anchor at negative displacements.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_wldcore_menu_stack_depth after each call. */
void wldcore_menu_push_script_flag_01a4_detail_level(s32 value) {
    wldcore_text_scrollable_window_t* record;
    s32* anchor;

    anchor = &g_wldcore_scrollable_text_window.text_id;
    /* Hides the anchor from cse (see above). */
    __asm__("" : "=r"(anchor) : "0"(anchor));

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_index = value;

    record = (wldcore_text_scrollable_window_t*)(anchor - 13);

    record->text_id = value + 0x8800;
    record->image_x = 0;
    g_wldcore_scrollable_text_window.image_y = 0x70;
    record->base.x = -122;
    record->base.y = -40;
    record->text_width = 0xdc;
    record->rows_per_page = 8;
    record->priority = 9;
    record->image_coordinate_mode = 1;
    record->extra_render_index = -1;
    record->text_substitutions[1] = -1;
    record->text_substitutions[0] = -1;
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
    g_world_text_substitution_values[0] = value + 0x9800;
    world_thread_set_parameters(0xE, 0x19, 0xB849, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SCRIPT_FLAG_01A4_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
