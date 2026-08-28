#include "fft/open.h"
#include "psx/pad.h"
#include "psx/types.h"

void open_movie_finish_fftend_controller_if_ready_or_skipped(void) {
    u32 flags;
    s32 controller;

    if (!(g_open_system_runtime_flags & 2)) {
        SetDispMask(0);
        flags = g_open_system_runtime_flags;
        controller = g_open_current_controller_index;
        flags |= 0x40;
        controller--;
        g_open_system_runtime_flags = flags;
        g_open_current_controller_index = controller;
        open_script_push_controller();
        return;
    }

    if (g_open_input_new_button_presses & PSX_PAD_START) {
        open_movie_stop_stream(1);
    }
}
