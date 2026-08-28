#include "fft/wldcore.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Each block takes its own pointer local so the flag address is materialized
 * twice, as in the target. */
void wldcore_opcode_wait_for_circle_press(void) {
    if ((g_wldcore_new_button_presses & PSX_PAD_CIRCLE) != 0) {
        u16* state_flags;

        state_flags = &g_wldcore_active_saved_record.state_flags;
        *state_flags |= 4;
    }
    {
        u16* state_flags;

        state_flags = &g_wldcore_active_saved_record.state_flags;
        *state_flags |= 8;
    }
}
