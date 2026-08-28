#include "fft/wldcore.h"

/* Volatile views of the script state: required to keep the target's access order. */
extern volatile s32 g_wldcore_script_flags;
extern volatile u16 g_wldcore_script_ip;

/* Stage the two operands for the deferred SetVar2 request.
 *
 * This opcode only prepares the request; a later dispatcher performs it.
 */
void wldcore_opcode_load_set_var2_arguments(void) {
    u16 ip;
    volatile u16* data;
    s32 operand;
    s32 flags;

    ip = g_wldcore_script_ip;
    data = g_wldcore_script_state.data;
    operand = data[ip];
    g_wldcore_script_ip = ip + 1;
    g_wldcore_script_state.args[0] = operand;
    operand = data[(u16)(ip + 1)];
    flags = g_wldcore_script_flags;
    g_wldcore_script_ip = ip + 2;
    g_wldcore_script_flags = flags | WLDCORE_SCRIPT_REQUEST_SET_VAR_2;
    g_wldcore_script_state.args[1] = operand;
}
