#include "fft/open.h"
#include "psx/types.h"

void open_opcode_toggle_control(void) {
    u32* control = &g_open_script_state.dispatch.flags;
    u32 state = *control;

    if (state & 0x30) {
        *control = state ^ 2;
    } else {
        *control = state ^ 1;
        g_open_script_state.dispatch.byte_offset += 2;
    }
}
