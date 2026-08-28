#include "fft/thread.h"
#include "fft/world.h"
#include "psx/types.h"

/* Companion of world_bin_load_file: waits for the loader to go idle, posts
 * `request` through g_world_pending_bin_load_index and blocks until it is consumed. */
void world_bin_load_file_ptr(s32 request) {
    while (g_world_bin_load_request != 0 || g_world_pending_bin_load_index != 0) {
        world_thread_yield();
    }
    g_world_pending_bin_load_index = request;
    while (g_world_pending_bin_load_index != 0) {
        world_thread_yield();
    }
    if (g_world_frame_result_override == 1) {
        world_thread_exit_current();
    }
}
