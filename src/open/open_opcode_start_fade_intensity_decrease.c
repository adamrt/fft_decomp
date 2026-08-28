#include "fft/open.h"
#include "psx/types.h"

void open_opcode_start_fade_intensity_decrease(const s16* command) {
    s32 value = command[1];
    u32* control = &g_open_script_state.dispatch.flags;
    u32 flags;
    s32 next;

    g_open_script_state.opntex.fade_elapsed_frames = 0;
    g_open_script_state.opntex.fade_intensity = 0x80;
    flags = *control;
    next = g_open_script_state.dispatch.byte_offset;
    *control = flags | 0x80;
    g_open_script_state.opntex.fade_duration_frames = value;
    g_open_script_state.dispatch.byte_offset = next + 4;
}
