#include "fft/wldcore.h"

/* Apply and clear the pending signed delta, then publish a bounded value.
 *
 * The consumer requires a 16-bit unsigned range. State flag 0x04 marks the
 * update complete; the surrounding dispatch table has not identified this
 * handler's script opcode yet. */
void wldcore_opcode_apply_pending_delta_and_clamp_value(void) {
    s32 delta;
    s32 current;
    s32* value = &g_wldcore_active_saved_record.counter;

    delta = g_wldcore_active_saved_record.counter_delta;
    g_wldcore_active_saved_record.counter_delta = 0;
    current = *value;
    *value = current + delta;
    if (*value > 0xFFFF) {
        *value = 0xFFFF;
    }
    if (*value < 0) {
        *value = 0;
    }
    g_wldcore_displayed_numeric_value = *value;
    g_wldcore_active_saved_record.state_flags |= 4;
}
