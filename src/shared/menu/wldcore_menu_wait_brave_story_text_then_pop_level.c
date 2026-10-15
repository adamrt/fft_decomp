#include "fft/wldcore.h"

/* Return from a message level to the window/render level beneath it.
 *
 * Waits until the message render record has left its 0x100 state and
 * wldcore_text_is_window_finished reports the message window finished, then clears flag 0x10 on
 * the parent level's window and render records, pops the stack and marks the
 * render record with 0x100 before wldcore_list_open_brave_story_propositions redraws. Mirrors
 * wldcore_menu_wait_text_then_pop_level. */
void wldcore_menu_wait_brave_story_text_then_pop_level(void) {
    s32* index = &g_wldcore_scrollable_text_window.render_record_index;
    s32 depth;

    if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_render.window_index].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_render.render_index].flags
        &= ~WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth = depth - 1;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_render.render_index].flags
        |= WORLD_RENDER_RECORD_POP_IN;
    wldcore_list_open_brave_story_propositions();
}
