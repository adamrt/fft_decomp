#include "fft/script_variables.h"
#include "fft/wldcore.h"
#include "fft/world.h"
#include "psx/types.h"

/* Branches the sound-novel script to section g_wldcore_active_saved_record.instruction.bytes.operand_0 when the
 * selected dialog option equals operand byte 2 (or differs from it, when `branch_if_equal` is 0); otherwise raises
 * state flag 4. */
void wldcore_opcode_branch_if_dialog_option(s32 branch_if_equal) {
    u8 operand = g_wldcore_active_saved_record.instruction.bytes.operand_1;

    if (operand == world_script_get_variable(EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION) ? branch_if_equal == 1
                                                                                      : branch_if_equal == 0) {
        g_wldcore_active_saved_record.section = g_wldcore_active_saved_record.instruction.bytes.operand_0;
        g_wldcore_active_saved_record.state_flags |= 2;
    } else {
        u16* flags = &g_wldcore_active_saved_record.state_flags;

        *flags |= 4;
    }
}
