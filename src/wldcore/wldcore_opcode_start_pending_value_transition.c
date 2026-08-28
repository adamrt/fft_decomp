#include "fft/main_runtime.h"
#include "fft/wldcore.h"

void wldcore_opcode_start_pending_value_transition(void) {
    s32 mode;

    g_main_system_flags &= ~0x800;
    mode = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    g_wldcore_active_saved_record.value_display_mode = mode;
    g_wldcore_context_value_display_mode = mode;
    if (mode != 2) {
        g_wldcore_displayed_numeric_value = g_wldcore_active_saved_record.counter;
    }
    g_wldcore_context_value_display_position.x = -0x20;
    g_wldcore_context_value_display_position.y = 0x54;
    /* The target stores the whole word (sw), not the u16 the sorts read. */
    *(s32*)&g_wldcore_hud_ot_priority = 0xa;
    g_wldcore_active_saved_record.state_flags |= 0x84;
}
