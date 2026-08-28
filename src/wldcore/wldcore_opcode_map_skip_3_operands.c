#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_map_skip_3_operands(void) {
    u16* ip = &g_wldcore_script_state.ip;
    *ip += 3;
}
