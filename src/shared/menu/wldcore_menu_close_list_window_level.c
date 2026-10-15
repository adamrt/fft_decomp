#include "fft/thread.h"
#include "fft/wldcore.h"

/* Clears a list level's window states, restores its hidden records, and
 * queues WORLD thread 14 with message 0xb848. */
void wldcore_menu_close_list_window_level(wldcore_menu_stack_record_t* level) {
    g_wldcore_window_records[level->list_window.main_window].palette
        = g_wldcore_window_records[level->list_window.side_window].palette
        = g_wldcore_window_render_records[level->list_window.frame_render].palette
        = g_wldcore_window_render_records[level->list_window.content_render].palette = 0;
    g_wldcore_window_records[level->list_window.upper_window].palette
        = g_wldcore_window_records[level->list_window.lower_window].palette = 0;
    g_wldcore_window_records[level->list_window.main_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[level->list_window.main_window].frame_index
        = g_wldcore_window_records[level->list_window.main_window].anim_counter = 0;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    if (g_wldcore_window_render_records[level->list_window.frame_render].flags & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_records[level->list_window.main_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->list_window.side_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_window.frame_render].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[level->list_window.frame_render].flags |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_records[level->list_window.upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->list_window.lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    if (g_wldcore_window_render_records[level->list_window.content_render].flags & WORLD_RENDER_RECORD_HIDDEN) {
        g_wldcore_window_render_records[level->list_window.content_render].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
        g_wldcore_window_render_records[level->list_window.content_render].flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    level->list_window.mode = WLDCORE_LIST_WINDOW_MODE_BROWSE;
}
