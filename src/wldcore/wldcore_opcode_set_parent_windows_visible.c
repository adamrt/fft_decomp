#include "fft/wldcore.h"

/* Script opcode: operand byte 0 sets (non-zero) or clears state flag 0x40
 * and shows or hides both windows of the parent menu level (window flag
 * 0x10), then sets state flag 0x04. */
void wldcore_opcode_set_parent_windows_visible(void) {
    if (g_wldcore_opcode_state.instruction.bytes.operand_0 != 0) {
        g_wldcore_opcode_state.state_flags |= 0x40;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.first_window]
            .flags &= ~0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.second_window]
            .flags &= ~0x10;
    } else {
        g_wldcore_opcode_state.state_flags &= ~0x40;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.first_window]
            .flags |= 0x10;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                     .window_pair.second_window]
            .flags |= 0x10;
    }
    g_wldcore_opcode_state.state_flags |= 4;
}
