#include "fft/wldcore.h"

/* Sound-novel opcode: load background set operand 0 unless it is the one
 * already recorded, waiting for the file. */
void wldcore_opcode_load_background_set(void) {
    s32 value;

    value = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    if (g_wldcore_active_saved_record.background_set != value) {
        DrawSync(0);
        g_wldcore_active_saved_record.background_set = value;
        wldcore_gfx_load_background_set(value);
        file_wait_for_pending_variant();
    }
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}
