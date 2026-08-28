#include "psx/types.h"

void wldcore_opcode_branch_if_saved_bit_clear(void) {
    wldcore_opcode_branch_if_saved_bit(0);
}
