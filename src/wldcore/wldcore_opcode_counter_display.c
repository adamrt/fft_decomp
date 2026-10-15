/* Counter set/add/subtract and the value display show/hide. */

#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_set_counter(void) {
    g_wldcore_active_saved_record.counter = (g_wldcore_opcode_state.instruction << 8) >> 16;
    g_wldcore_state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: add the 16-bit operand (instruction bits 8..23) to the
 * record's counter delta. */
void wldcore_opcode_add_counter_delta(void) {
    g_wldcore_active_saved_record.counter_delta += (g_wldcore_opcode_state.instruction << 8) >> 16;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: subtract the 16-bit operand (instruction bits 8..23)
 * from the record's counter delta. */
void wldcore_opcode_subtract_counter_delta(void) {
    g_wldcore_active_saved_record.counter_delta -= (g_wldcore_opcode_state.instruction << 8) >> 16;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH;
}

/* Sound-novel opcode: show the context value display in the mode of operand
 * 0 at (-0x20, 0x54). Any mode but the play time shows the record's counter. */
void wldcore_opcode_start_pending_value_transition(void) {
    s32 mode;

    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    mode = (u8)(g_wldcore_active_saved_record.instruction >> 8);
    g_wldcore_active_saved_record.value_display_mode = mode;
    g_wldcore_context_value_display_mode = mode;
    if (mode != WLDCORE_VALUE_DISPLAY_PLAY_TIME) {
        g_wldcore_displayed_numeric_value = g_wldcore_active_saved_record.counter;
    }
    g_wldcore_context_value_display_position.x = -0x20;
    g_wldcore_context_value_display_position.y = 0x54;
    g_wldcore_hud_ot_priority = 0xa;
    g_wldcore_active_saved_record.state_flags |= WLDCORE_OPCODE_STATE_FETCH | WLDCORE_OPCODE_STATE_VALUE_DISPLAY;
}

/* Opcode 29: take the numeric value display down again (wldcore_opcode_start_pending_value_transition
 * puts it up) and fetch the next instruction. */
void wldcore_opcode_hide_value_display(void) {
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_active_saved_record.state_flags
        = (g_wldcore_active_saved_record.state_flags & ~WLDCORE_OPCODE_STATE_VALUE_DISPLAY)
        | WLDCORE_OPCODE_STATE_FETCH;
}
