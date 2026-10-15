#include "fft/thread.h"
#include "fft/wldcore.h"

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
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The depth is spelled as the global at every use rather than cached in a
 * local: the target re-reads g_wldcore_menu_stack_depth after each call.
 *
 * The frame is 32 bytes against the twin's 24: the target reserves eight bytes
 * below its two saved registers that it never touches, reproduced by the
 * unused local. */
void wldcore_menu_push_story_event_text_level(s32 value) {
    s32 unused[2];
    s32 coordinate_mode = 1;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].story_event_index = value;
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
    g_wldcore_scrollable_text_window.priority = 9;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[1] = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = -1;
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
    g_text_substitution_values[0] = value + TEXT_ID_STORY_EVENT_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_STORY_EVENT_TEXT;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}
