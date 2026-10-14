#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Built at -O1, like its neighbors world_menu_toggle_thread_9 /
 * world_menu_toggle_thread_10_alt. */

void world_menu_toggle_preview_stats_window(s32 flag) {
    s32 stored;

    stored = flag;
    if (stored != 0) {
        stored = 1;
        if (!(g_world_status_display_thread_params.flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP))
            && thread_is_running(WORLD_THREAD_SLOT_PREVIEW_STATS) == 0) {
            thread_start(WORLD_THREAD_SLOT_PREVIEW_STATS, world_menu_run_numeric_editor_thread);
            thread_set_parameters(WORLD_THREAD_SLOT_PREVIEW_STATS, (s32)&g_world_preview_stats_thread_params, 0, 0);
        }
    } else {
        world_menu_stop_unit_status_banner_thread(WORLD_THREAD_SLOT_PREVIEW_STATS);
    }
    g_world_numeric_editor_thread_params.style = stored;
    g_world_preview_stats_window_active = stored;
}
