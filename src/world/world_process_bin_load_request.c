#include "fft/main_runtime.h"
#include "fft/world.h"

extern u8* g_event_overlay_load_address;

/* Per-frame driver for world_bin_load_file: start the disc read for the
 * pending request on the main thread, then poll until it finishes and clear
 * the request. */
void world_process_bin_load_request(void) {
    s32 request;

    request = g_world_bin_load_request;
    if (request != 0 && g_world_bin_load_in_progress == 0) {
        g_world_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (world_thread_call_on_main_stack(g_world_bin_load_sectors[request], g_world_bin_load_sizes[request],
                g_world_bin_load_address_offsets[request] + (s32)g_event_overlay_load_address)
            == 0) {
            g_world_bin_load_in_progress = 1;
        }
    } else if (g_world_bin_load_request != 0 && g_world_bin_load_in_progress != 0) {
        g_world_thread_call_target = main_file_is_still_loading;
        if (world_thread_call_on_main_stack() == 0) {
            g_world_bin_load_in_progress = 0;
            g_world_bin_load_request = 0;
        }
    }
}
