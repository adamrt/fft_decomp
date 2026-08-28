#include "fft/main_runtime.h"
#include "fft/wldcore.h"

/* Ends the window tint transition and restores the parent content record.
 *
 * Cache the request's window and render indices before the state stores:
 * those stores can alias the request. */
void wldcore_window_end_tint_transition(wldcore_window_tint_request_t* request) {
    wldcore_window_render_record_t* render;
    s32 window = request->window_a;
    s32 render_index = request->render_a;

    render = g_wldcore_window_render_records;
    /* Keeps the render-record base in a2 for the first two stores; GCC folds it to $at. */
    __asm__("" : : "r"(render));
    render[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2].list_window.content_render].palette = 0;
    render[render_index].palette = 0;
    g_wldcore_window_records[window].palette = 0;
    g_wldcore_window_records[request->window_a].sequence = 2;
    g_wldcore_window_records[request->window_a].frame_index = g_wldcore_window_records[request->window_a].anim_counter
        = 0;
    if (g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .flags
        & 0x10) {
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .flags &= ~0x10;
        g_main_system_flags &= ~0x800;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 2]
                                            .list_window.content_render]
            .flags |= 0x100;
    }
}
