#include "fft/wldcore.h"

/* Volatile views of the script state: required to keep the target's access order. */
extern volatile s32 g_wldcore_script_flags;
extern u16* volatile g_wldcore_script_data_ptr;
extern volatile u16 g_wldcore_script_ip;

/* Stage the three operands for a deep-dungeon entry request.
 *
 * Each operand advances the script IP before the request becomes ready.
 */
void wldcore_opcode_load_deep_dungeon_entry_arguments(void) {
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
    g_wldcore_script_ip = ip + 2;
    g_wldcore_script_state.args[1] = operand;
    operand = data[(u16)(ip + 2)];
    flags = g_wldcore_script_flags;
    g_wldcore_script_ip = ip + 3;
    g_wldcore_script_flags = flags | WLDCORE_SCRIPT_REQUEST_DEEP_DUNGEON_ENTRY;
    g_wldcore_script_state.args[2] = operand;
}
