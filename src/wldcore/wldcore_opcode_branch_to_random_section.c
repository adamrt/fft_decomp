#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_branch_to_random_section(void) {
    s32 pick;
    s32 value;

    pick = (rand() * 3) >> 15;
    if (pick == 0) {
        value = g_wldcore_active_saved_record.instruction.bytes.operand_0;
    }
    if (pick == 1) {
        value = g_wldcore_active_saved_record.instruction.bytes.operand_1;
    }
    if (pick == 2) {
        value = g_wldcore_active_saved_record.instruction.bytes.operand_2;
    }
    g_wldcore_active_saved_record.section = value;
    g_wldcore_active_saved_record.state_flags |= 2;
}
