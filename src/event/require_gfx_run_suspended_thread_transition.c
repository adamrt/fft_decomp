#include "fft/event_require.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Thread body: suspend thread g_require_thread_suspended_id and draw the
 * gradient grid and status group primitives every frame in its place until
 * the third thread parameter is set.
 *
 * The suspended thread's record is copied aside first and copied back before
 * the thread is resumed.
 */
void require_gfx_run_suspended_thread_transition(void) {
    s32 frame = 0;
    u8* saved;

    thread_suspend(g_require_thread_suspended_id);
    saved = menu_alloc_memory(THREAD_STRIDE);
    mem_copy_bytes_nonempty(saved, &g_threads[g_require_thread_suspended_id], THREAD_STRIDE);
    g_menu_panel_status_group_count = 2;
    while (1) {
        s32 buffer = frame & 1;
        require_gfx_build_gradient_grid_primitives(g_require_gfx_poly_gt4_buffers[buffer]);
        menu_build_status_group_primitives(g_require_gfx_poly_ft4_buffers[buffer]);
        thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    mem_copy_bytes_nonempty(&g_threads[g_require_thread_suspended_id], saved, THREAD_STRIDE);
    menu_free_memory(saved);
    thread_resume(g_require_thread_suspended_id);
    thread_yield();
    thread_exit_current();
}
