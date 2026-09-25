#include "fft/wldcore.h"

/* Reads 134 sectors from LBA 0x11d28 into the wldcore scratch buffer unless
 * system flag 0x10000 is set, then spins the overlay's own poll routine until
 * the transfer completes.
 *
 * The target frame is 0x20 rather than the 0x18 this body needs: 16 bytes of
 * outgoing arguments plus the ra/s0 save pair leave 8 bytes of unreferenced
 * locals, so the original declared an 8-byte stack object it never used. Its
 * type is unknown; the array below reproduces the slot exactly. */
void wldcore_load_scratch_data_blocking(void) {
    s32 unused_stack_slot[2];

    if ((g_main_system_flags & 0x10000) == 0) {
        wldcore_wait_and_build_file_header(&g_main_file_cd_state, 0x11d28, 0x86, g_wldcore_scratch_buffer);
        while (g_main_file_cd_state.state != MAIN_FILE_LOAD_STATE_IDLE) {
            wldcore_file_poll_vram_image_stream(&g_main_file_cd_state);
        }
    }
}
