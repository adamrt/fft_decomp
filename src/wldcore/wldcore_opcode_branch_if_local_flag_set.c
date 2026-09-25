#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_branch_if_local_flag_set(void) {
    wldcore_opcode_branch_if_local_flag(1);
}
