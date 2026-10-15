#include "fft/script.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/pad.h"

/* Close this menu level once its message window is finished.
 *
 * Same wait as wldcore_menu_wait_text_then_pop_level; then toggles flag 0x10 on the
 * level's two windows and render record, pops the stack and marks the render
 * record 0x100 before wldcore_list_open_rumors redraws. */
void wldcore_bar_wait_rumor_text_then_pop_level(wldcore_menu_rumor_detail_level_t* level) {
    s32* index = &g_wldcore_scrollable_text_window.render_record_index;

    if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    g_wldcore_window_records[level->first_window].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[level->second_window].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[level->render_index].flags ^= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_menu_stack_depth--;
    g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_POP_IN;
    wldcore_list_open_rumors();
}
