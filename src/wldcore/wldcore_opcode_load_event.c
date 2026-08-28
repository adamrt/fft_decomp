#include "fft/wldcore.h"

/* Volatile views of the script state: required to keep the target's access order. */
extern volatile s32 g_wldcore_script_flags;
extern u16* volatile g_wldcore_script_data_ptr;
extern volatile u16 g_wldcore_script_ip;

/* Stage the two-word event identifier for a deferred LoadEvent request.
 *
 * Treating the adjacent words as elements of one array preserves the original
 * store order and register allocation.
 */
void wldcore_opcode_load_event(void) {
    u16 ip;
    volatile u16* data;
    s32 first_operand;
    s32 second_operand;
    s32 flags;

    ip = g_wldcore_script_ip;
    data = (volatile u16*)g_wldcore_script_data_ptr;
    first_operand = data[ip];
    g_wldcore_script_ip = ip + 1;
    g_wldcore_script_state.args[0] = first_operand;
    second_operand = data[(u16)(ip + 1)];
    flags = g_wldcore_script_flags;
    g_wldcore_script_ip = ip + 2;
    g_wldcore_script_flags = flags | WLDCORE_SCRIPT_REQUEST_LOAD_EVENT;
    g_wldcore_script_state.args[1] = second_operand;
}
