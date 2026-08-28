#include "fft/main_runtime.h"
#include "fft/require.h"
#include "fft/thread.h"
#include "psx/types.h"

void require_gfx_run_suspended_thread_transition(void) {
    s32 frame = 0;
    u8* saved;

    battle_thread_suspend(g_require_thread_suspended_id);
    saved = battle_menu_alloc_memory(NATIVE_THREAD_STRIDE);
    battle_copy_bytes(
        saved, (u8*)g_battle_threads + g_require_thread_suspended_id * NATIVE_THREAD_STRIDE, NATIVE_THREAD_STRIDE);
    g_require_panel_status_group_count = 2;
    while (1) {
        s32 buffer = frame & 1;
        require_gfx_build_gradient_grid_primitives(g_require_gfx_poly_gt4_banks[buffer]);
        require_gfx_build_status_group_primitives((POLY_FT4*)(g_require_gfx_poly_ft4_banks + buffer * 0x4b0));
        battle_thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    battle_copy_bytes(
        (u8*)g_battle_threads + g_require_thread_suspended_id * NATIVE_THREAD_STRIDE, saved, NATIVE_THREAD_STRIDE);
    battle_menu_free_memory(saved);
    battle_thread_resume(g_require_thread_suspended_id);
    battle_thread_yield();
    battle_thread_exit_current();
}
