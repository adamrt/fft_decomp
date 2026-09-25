#include "fft/wldcore.h"

/* Initialize panel state and start its WORLD rendering thread. */
void wldcore_window_start_panel_render_thread(
    s32 thread_id, s32 extent, wldcore_point32_t* dimensions, wldcore_point32_t* origin) {
    wldcore_window_init_panel_render_state(thread_id, extent, dimensions, origin);
    world_thread_start(thread_id, world_build_at_list_2);
    world_thread_set_parameters(thread_id, &g_wldcore_window_panel_render_state, 0, 0);
}
