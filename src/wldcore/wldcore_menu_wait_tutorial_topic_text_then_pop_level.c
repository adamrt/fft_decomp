#include "fft/wldcore.h"

/* Tutorial topic text window set up by 0x80084578. */

/* Menu level 0x22 pushed by 0x80084578: steps the tutorial text window
 * until 0x800903e4 reports it finished, then pops the level, restores the
 * parent level's windows and render record, and reopens the topic list. */
void wldcore_menu_wait_tutorial_topic_text_then_pop_level(void) {
    s32 depth;

    if (!(g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags & 0x100)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.render_index]
            .flags |= 0x100;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window].flags
            &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.second_window].flags
            &= ~0x10;
        g_wldcore_menu_stack_depth = depth - 1;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.render_index]
            .flags &= ~0x10;
        wldcore_list_open_tutorial_topics();
    }
}
