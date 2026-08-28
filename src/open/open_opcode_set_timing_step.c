#include "fft/open.h"
#include "psx/types.h"

void open_opcode_set_timing_step(const s16* command) {
    s32 value = command[1];
    s32 next = g_open_script_byte_offset + 4;

    g_open_script_state.dispatch.external_counter = 0;
    g_open_script_state.dispatch.timing_fraction = 0;
    g_open_script_state.dispatch.timing_step = value;
    g_open_script_byte_offset = next;
}
