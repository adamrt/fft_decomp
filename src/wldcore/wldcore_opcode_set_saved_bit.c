#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_set_saved_bit(void) {
    wldcore_set_bit_value(
        g_main_secondary_saved_data_bits, g_wldcore_active_saved_record.instruction.bytes.operand_0, 1);
    g_wldcore_active_saved_record.state_flags |= 4;
}
