#include "fft/open.h"
#include "psx/types.h"

void open_opcode_start_opntex_sequence(const s16* command) {
    s32 final_value;
    s32 third_value;
    s32 active;
    s32 script_offset;

    command++;
    g_open_script_state.opntex.first_frame = *command++;
    g_open_script_state.opntex.last_frame = *command++;
    third_value = *command++;
    g_open_script_state.opntex.frame_duration = third_value;
    final_value = *command;
    script_offset = g_open_script_state.dispatch.byte_offset;
    active = 1;
    g_open_script_state.opntex.frame_countdown = active;
    g_open_script_state.dispatch.byte_offset = script_offset + 10;
    g_open_script_state.opntex_control_value = final_value;
}
