#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_branch_if_saved_bit_set(void) {
    wldcore_opcode_branch_if_saved_bit(1);
}
