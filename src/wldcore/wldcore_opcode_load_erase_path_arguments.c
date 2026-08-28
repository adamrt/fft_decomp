#include "fft/wldcore.h"

/* Volatile views of the script state: required to keep the target's access order. */
extern volatile s32 g_wldcore_script_flags;
extern u16* volatile g_wldcore_script_data_ptr;
extern volatile u16 g_wldcore_script_ip;

/* Stage the two operands for a deferred ErasePath request.
 *
 * The request flag is published after both operands and IP updates.
 */
void wldcore_opcode_load_erase_path_arguments(void) {
    u16 ip;
    volatile u16* data;
    s32 operand;
    s32 flags;

    ip = g_wldcore_script_ip;
    data = (volatile u16*)g_wldcore_script_data_ptr;
    operand = data[ip];
    g_wldcore_script_ip = ip + 1;
    g_wldcore_script_state.args[0] = operand;
    operand = data[(u16)(ip + 1)];
    flags = g_wldcore_script_flags;
    g_wldcore_script_ip = ip + 2;
    g_wldcore_script_flags = flags | WLDCORE_SCRIPT_REQUEST_ERASE_PATH;
    g_wldcore_script_state.args[1] = operand;
}
