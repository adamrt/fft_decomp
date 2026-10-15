#include "fft/thread.h"
#include "fft/wldcore.h"

/* Initialize panel state and start its WORLD rendering thread. */
void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_window_panel_layout_t* layout, point32_t* origin) {
    wldcore_window_init_panel_render_state(thread_id, extent, layout, origin);
    thread_start(thread_id, world_screen_build_at_list);
    thread_set_parameters(thread_id, &g_wldcore_window_panel_render_state, 0, 0);
}
