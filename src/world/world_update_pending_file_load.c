#include "fft/main_runtime.h"
#include "fft/world.h"
#include "psx/types.h"

/* Drives the file load requested through g_world_pending_bin_load_index: starts the disc read
 * on the main thread, then polls it until it finishes and clears the
 * request. */
void world_update_pending_file_load(void) {
    s32 idx = g_world_pending_bin_load_index;

    if (idx != 0 && g_world_pending_bin_load_in_progress == 0) {
        g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_call_build_header;
        if (world_thread_call_on_main_stack(g_world_pending_bin_load_sectors[idx], g_world_pending_bin_load_sizes[idx],
                g_world_pending_bin_load_destination)
            == 0) {
            g_world_pending_bin_load_in_progress = 1;
        }
    } else if (g_world_pending_bin_load_index != 0 && g_world_pending_bin_load_in_progress != 0) {
        g_world_thread_inner_subroutine_callback = main_file_is_still_loading;
        if (world_thread_call_on_main_stack() == 0) {
            g_world_pending_bin_load_in_progress = 0;
            g_world_pending_bin_load_index = 0;
        }
    }
}
