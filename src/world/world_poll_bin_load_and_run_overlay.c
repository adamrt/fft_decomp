#include "fft/small.h"
#include "fft/world.h"
#include "psx/types.h"

/* Drive the pending BIN load and, once it completes, run the SMALL.OUT entry.
 *
 * Only request 8 (world_request_bin_file_load_8) places a file at 0x801d7000:
 * SMALL.OUT, at overlay offset 0x18000. */
s32 world_poll_bin_load_and_run_overlay(void) {
    if (g_world_bin_load_request == 0) {
        return 0;
    }
    world_process_bin_load_request();
    if (g_world_bin_load_request != 0) {
        return 1;
    }
    small_text_init_battle_pointers();
    return 0;
}
