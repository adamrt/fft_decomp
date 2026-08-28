#include "fft/wldcore.h"

void wldcore_script_read_operand_pair(s32* out1, s32* out2) {
    u16* ip = &g_wldcore_script_state.ip;
    u16 next_ip;

    *out1 = ((u16*)g_wldcore_script_state.data)[*ip];
    next_ip = *ip + 1;
    *ip = next_ip;
    *out2 = ((u16*)g_wldcore_script_state.data)[next_ip];
    *ip = *ip + 1;
}
