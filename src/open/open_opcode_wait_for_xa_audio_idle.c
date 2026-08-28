#include "fft/open.h"
#include "psx/types.h"

void open_opcode_wait_for_xa_audio_idle(void) {
    u32* control = &g_open_script_state.dispatch.flags;
    u32 state = *control;

    if (state & 0x38) {
        *control = state ^ 2;
    } else {
        g_open_script_state.dispatch.byte_offset += 2;
    }
}
