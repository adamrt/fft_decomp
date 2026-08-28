#include "fft/open.h"

void open_opcode_start_xa_audio(const u8* command) {
    u32* control = &g_open_script_state.dispatch.flags;
    u32 state = *control;

    if (state & 4) {
        if (state & 0x20) {
            *control = state ^ 2;
            return;
        }
        {
            s32 script_offset = g_open_script_state.dispatch.byte_offset;

            *control = state ^ 4;
            g_open_script_state.dispatch.byte_offset = script_offset + 4;
            return;
        }
    }
    if (state & 0x38) {
        s32 script_offset = g_open_script_state.dispatch.byte_offset;

        g_open_script_state.dispatch.byte_offset = script_offset + 4;
        return;
    }

    {
        s32 parameter = *(const s16*)(command + 2);

        g_open_script_state.control_state = 0;
        *control = (state & ~0x3a) | 0x24;
        g_open_script_state.control_parameter = parameter;
        open_sound_set_type_and_volume(0, 1);
    }
}
