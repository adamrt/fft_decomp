#include "fft/wldcore.h"
#include "psx/types.h"

/* Reject the Var= condition when the script variable differs from the operand. */
void wldcore_opcode_map_check_var_equal(void) {
    s32 variable_id;
    s32 value;
    s32* flags;

    wldcore_script_read_operand_pair(&variable_id, &value);
    if (world_script_get_variable(variable_id) != value) {
        flags = &g_wldcore_script_state.flags;
        *flags |= WLDCORE_SCRIPT_RESULT_CONDITION_FAILED;
    }
}
