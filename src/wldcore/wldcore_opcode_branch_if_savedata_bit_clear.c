#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_branch_if_savedata_bit_clear(void) {
    wldcore_opcode_branch_if_savedata_bit(0);
}
