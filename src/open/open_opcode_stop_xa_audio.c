#include "fft/open.h"

void open_opcode_stop_xa_audio(void) {
    u32* control = &g_open_script_state.dispatch.flags;
    u32 state = *control;

    if (state & 4) {
        if (state & 0x10) {
            *control = state ^ 2;
        } else {
            *control = state ^ 4;
            g_open_script_state.dispatch.byte_offset += 2;
        }
    } else if (!(state & 0x28)) {
        g_open_script_state.dispatch.byte_offset += 2;
    } else {
        g_open_script_state.control_state = 0;
        *control = (state & ~0x3a) | 0x14;
        open_sound_set_type_and_volume(0, 1);
    }
}
