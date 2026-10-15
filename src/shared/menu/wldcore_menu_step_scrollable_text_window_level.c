#include "fft/wldcore.h"

/* Menu-level step for the scrollable text window at 0x8009eef8. While its
 * render record is hidden (flag 0x10), an unblocked frame plays sound 0x12
 * and shows the level window and the render record again. Otherwise, once
 * 0x800903e4 reports the window finished, it drops one window and one render
 * object, pops the level, clears system flags 0x08002000, shows the parent
 * level's window and resumes the bar menu. */
void wldcore_menu_step_scrollable_text_window_level(wldcore_menu_stack_record_t* level) {
    s32* index = &g_wldcore_scrollable_text_window.render_record_index;
    s32 depth;

    if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_HIDDEN) {
        if (!(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
            wldcore_sound_play_effect(MAIN_SFX_WINDOW_OPEN);
            g_wldcore_window_records[level->window_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[*index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    } else if (!(g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN)
        && wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
        g_wldcore_window_record_count--;
        g_wldcore_window_render_object_count--;
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_menu_stack_depth = depth - 1;
        g_main_system_flags &= ~(MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER | MAIN_SYSTEM_FLAG_FREEZE_ANIMATION);
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_index].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        wldcore_menu_dispatch_resume_handler();
    }
}
