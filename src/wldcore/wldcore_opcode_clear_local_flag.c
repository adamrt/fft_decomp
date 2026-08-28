#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_opcode_clear_local_flag(void) {
    wldcore_set_bit_value(
        g_wldcore_sound_novel_local_flags, g_wldcore_active_saved_record.instruction.bytes.operand_0, 0);
    g_wldcore_active_saved_record.state_flags |= 4;
}
