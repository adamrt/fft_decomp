#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_branch_to_section(void) {
    u16 flags;

    flags = g_wldcore_state_flags | 2;
    g_wldcore_active_saved_record.section = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    g_wldcore_state_flags = flags;
}
