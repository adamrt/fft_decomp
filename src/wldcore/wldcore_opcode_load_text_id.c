#include "fft/wldcore.h"

/* Volatile views of the script state: required to keep the target's access order. */
extern u16* volatile g_wldcore_script_data_ptr;
extern volatile u16 g_wldcore_script_ip;

/* Stage the text ID consumed by the deferred world-script text request.
 *
 * The volatile instruction stream preserves the target's read-before-IP-write
 * order.
 */
void wldcore_opcode_load_text_id(void) {
    u16 ip;
    volatile u16* data;
    u16 value;
    s32 flags;

    ip = g_wldcore_script_ip;
    data = (volatile u16*)g_wldcore_script_data_ptr;
    value = data[ip];
    flags = g_wldcore_script_flags;
    g_wldcore_script_ip = ip + 1;
    g_wldcore_script_flags = flags | WLDCORE_SCRIPT_REQUEST_TEXT;
    g_wldcore_script_state.args[0] = value;
}
