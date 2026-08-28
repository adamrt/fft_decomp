#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

void open_movie_finish_controller_1_if_ready_or_skipped(void) {
    u32 flags;
    s32 controller;

    if (!(g_open_system_runtime_flags & 2)) {
        SetDispMask(0);
        flags = g_open_system_runtime_flags;
        controller = g_open_current_controller_index;
        g_open_system_result = 0;
        flags |= 0x40;
        controller--;
        g_open_system_runtime_flags = flags;
        flags ^= 1;
        g_open_current_controller_index = controller;
        g_open_system_runtime_flags = flags;
        return;
    }

    if (g_open_input_new_button_presses & PSX_PAD_START) {
        open_movie_stop_stream(1);
    }
}

/* State one stops an active stream on Start and performs its pop on the */
/* following update, after movie flag 2 has cleared.                     */
