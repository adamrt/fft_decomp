#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Activates the window pair named by a tint request: the window record takes
 * palette 10 and its render record palette 2, the window's three scratch words are
 * cleared, and the current menu level's list content render record is hidden
 * while the transition runs. */
void wldcore_window_begin_tint_transition(wldcore_window_tint_request_t* request) {
    g_wldcore_window_records[request->window_a].palette = 10;
    g_wldcore_window_render_records[request->render_a].palette = 2;
    g_wldcore_window_records[request->window_a].sequence = 0;
    g_wldcore_window_records[request->window_a].frame_index = g_wldcore_window_records[request->window_a].anim_counter
        = 0;
    g_main_system_flags |= 0x800;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                        .list_window.content_render]
        .flags |= 0x10;
}
